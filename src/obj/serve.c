// -*- c-basic-offset: 2; indent-tabs-mode: nil -*-

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <stdbool.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <fcntl.h>

typedef ssize_t sender_func(int tofd, int fromfd, size_t rem);

static sender_func use_splice, use_sendfile;
  
static ssize_t use_splice(int tofd, int fromfd, size_t rem)
{
  return splice(fromfd, NULL, tofd, NULL, rem, 0);
}

static ssize_t use_sendfile(int tofd, int fromfd, size_t rem)
{
  return sendfile(tofd, fromfd, NULL, rem);
}

static bool check_image_name(const char *s)
{
  if (*s++ != 'a') return false;
  if (*s++ != 't') return false;
  if (*s++ != '-') return false;
  const char *const p = s;
  while (*s >= '0' && *s <= '9')
    s++;
  if (s - p == 0) return false;
  if (*s++ != '.') return false;
  if (*s++ != 'j') return false;
  if (*s++ != 'p') return false;
  if (*s++ != 'g') return false;
  return true;
}

int main(int argc, const char *const *argv)
{
  const char *boundary = "kasduyc69c34ivkcuqbnqx4rkaghsjhcbasjcj";

  const char *workdir = getenv("WORKDIR");
  if (workdir == NULL) {
    fprintf(stderr, "%s: WORKDIR not set\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  /* Prepare a buffer to hold complete pathnames of detected
     changes. */
  char path[NAME_MAX];
  strcpy(path, workdir);
  strcat(path, "/");
  const size_t sfxpos = strlen(path);

  /* Prepare to watch for events. */
  int ino = inotify_init();
  if (ino < 0) {
    fprintf(stderr, "%s: %s: opening notifier\n", argv[0], strerror(errno));
    exit(EXIT_FAILURE);
  }

  /* Watch for events on the work directory. */
  int nwd = inotify_add_watch(ino, workdir, IN_CREATE | IN_MOVED_TO);
  if (nwd < 0) {
    fprintf(stderr, "%s: %s: watching %s\n",
            argv[0], strerror(errno), workdir);
    exit(EXIT_FAILURE);
  }

  /* Send an NPH CGI header. */
  printf("%s 200 Okay\r\n", getenv("SERVER_PROTOCOL"));
  //printf("Content-Type: text/plain\r\n\r\n");
  printf("Content-Type: multipart/x-mixed-replace; boundary=%s\r\n", boundary);
  printf("\r\n--%s", boundary);
  fflush(stdout);

  for ( ; ; ) {
    /* Read in events. */
    union {
      /* Union ensures alignment of byte array for structure. */
      struct inotify_event dummy;
      unsigned char bytes[sizeof(struct inotify_event) + NAME_MAX + 1];
    } buf;
    ssize_t got = read(ino, &buf, sizeof buf);
    if (got < 0) {
      fprintf(stderr, "%s: %s: reading event\n",
              argv[0], strerror(errno));
      exit(EXIT_FAILURE);
    }

    /* Parse the events to find the last one.  If we're not
       transmitting as fast as we're being notified, we have to drop
       frames anyway. */
    struct inotify_event *chosen = NULL;
    for (struct inotify_event *ptr = &buf.dummy;
         (unsigned char *) ptr - buf.bytes < got;
         ptr = (struct inotify_event *)
           &(ptr->len + sizeof *ptr)[(char *) ptr]) {
      if (ptr->len == 0) continue;
      if (!check_image_name(ptr->name)) continue;
      chosen = ptr;
    }
    if (chosen == NULL) continue;
    
    strncpy(path + sfxpos, chosen->name, sizeof path - sfxpos);
    //fprintf(stderr, "displaying %s...\n", chosen->name);

    /* Get the file's size. */
    struct stat mystat;
    if (stat(path, &mystat) < 0) {
      fprintf(stderr, "%s: %s: stat(%s)\n", argv[0], strerror(errno), path);
      exit(EXIT_FAILURE);
    }
    const size_t sz = mystat.st_size;

    /* Send this part's header. */
    printf("\r\n");
    printf("Content-Type: image/jpeg\r\n");
    printf("Content-Length: %zu\r\n", sz);
    printf("X-Timestamp: %ld.%09ld\r\n",
           mystat.st_mtim.tv_sec, mystat.st_mtim.tv_nsec);
    printf("\r\n");
    fflush(stdout);

    /* Send the file. */
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
      fprintf(stderr, "%s: %s: open(%s)\n", argv[0], strerror(errno), path);
      exit(EXIT_FAILURE);
    }

    struct scheme {
      const char *name;
      sender_func *func;
    } schemes[] = {
      { .name = "splice", .func = &use_splice },
      { .name = "sendfile", .func = &use_sendfile },
    };
    unsigned sn = 0;
    for (size_t rem = sz; rem > 0; ) {
      ssize_t done;
      while (sn < sizeof(schemes) / sizeof(schemes[0])) {
        done = (*schemes[sn].func)(fileno(stdout), fd, rem);
        if (done >= 0)
          break;
        if (errno != EINVAL) {
          fprintf(stderr, "%s: %s: %s(%s)\n",
                  argv[0], strerror(errno), schemes[sn].name, path);
          exit(EXIT_FAILURE);
        }
        sn++;
        continue;
      }
      assert(done >= 0);
      rem -= done;
    }
    close(fd);

    /* Send the boundary. */
    printf("\r\n--%s", boundary);
    fflush(stdout);
  }

  /* Terminate the multipart message.  We probably won't get here
     unless the watched directory is deleted. */
  printf("--\r\n");

  return EXIT_SUCCESS;
}
