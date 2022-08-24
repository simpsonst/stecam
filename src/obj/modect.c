// -*- c-basic-offset: 2; indent-tabs-mode: nil -*-

/*
 * Copyright 2018-19, Lancaster University
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 * 
 *  * Neither the name of the copyright holder nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Author: Steven Simpson <https://github.com/simpsonst>
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <string.h>
#include <stdbool.h>

#define ROOT2 (1.41421356237309504880)

struct vect {
  double x, y;
};

/* Make a parsable report to the user.  The first word is the score,
   or '-' if there is no score because the frame was not used for
   motion detection (indicated by 'fact<0').  The last word (to the
   end of the line) is the name of the source file associated with
   this score, surrounded by a couple of graphic characters.  Other
   words are additional diagnostic values. */
static void report(double mean, double sd, const char *line, double fact)
{
  static double rm = 0.0;
  const double alt = fdim((sd + 1e-7) / (mean + 1e-7), 1.0);
  rm = 0.8 * rm + 0.2 * alt;

  if (fact < 0.0)
    printf("- - - - X%sX\n", line);
  else {
#if 0
    printf("%.0f %g %g %g X%sX\n",
           (mean / 2.0 + sd / 8.0) * fact, mean, sd,
           rm, line);
#else
    printf("%.0f %g %g %g X%sX\n",
           rm * fact, mean, sd,
           rm, line);
#endif
  }
  fflush(stderr);
}

static double cos2vect(const struct vect *a, const struct vect *b)
{
  const double maga = a->x * a->x + a->y * a->y;
  const double magb = b->x * b->x + b->y * b->y;
  const double denom = sqrt(maga * magb);
  if (denom < 1e-6) return 0.0;

  const double numer = a->x * b->x + a->y * b->y;
  return (numer / denom + 1.0) / 2.0 * denom;
}

#if 0
static int digiclamp(double val)
{
  if (val < 0.0) return 0;
  if (val > 9.0) return 9;
  return val;
}
#endif

struct stats {
  unsigned sum;
  unsigned sum_sq;
};

static void add_image(int scale, unsigned width, unsigned height,
                      struct stats sum[height][width],
                      unsigned char (*img)[width])
{
  for (unsigned x = 0; x < width; x++)
    for (unsigned y = 0; y < height; y++) {
      unsigned val = img[y][x];
      sum[y][x].sum += scale * (int) val;
      sum[y][x].sum_sq += scale * (int) (val * val);
    }
}

#if 0
static int signum(int v)
{
  return (v > 0) - (v < 0);
}

static double detedge(signed short pix1, signed short pix2)
{
  if (signum(pix1) == signum(pix2)) return 0.0;
  return (double) pix1 * -pix2;
}
#endif

static int fsignum(double v)
{
  return (v > 0.0) - (v < 0.0);
}

static double fdetedge(double pix1, double pix2)
{
  if (fsignum(pix1) == fsignum(pix2)) return 0.0;
  return pix1 * -pix2;
}

static int read_filenames(FILE *namelist,
                          size_t linebuflen,
                          char linebuf[],
                          const char **srcnameptr,
                          const char **imgnameptr)
{
  if (fgets(linebuf, linebuflen, namelist) == NULL) return 0;

  char *srcname;
  unsigned long srclen = strtoul(linebuf, &srcname, 10);
  while (*srcname && isspace(*srcname))
    srcname++;

  /* Find the start of the name of the condensed file. */
  char *line = srcname + srclen;
  while (*line && isspace(*line))
    line++;

  /* Terminate the source file's name. */
  srcname[srclen] = '\0';

  /* Remove the condensed file's trailing newline. */
  size_t linelen = strlen(line);
  if (line[linelen - 1] == '\n')
    line[linelen - 1] = '\0';

  *srcnameptr = srcname;
  *imgnameptr = line;
  return 1;
}

/* Read in a frame from 'fin', and store its raw bytes in 'src'.
   Close 'fin'.  Return 0 on success; non-zero on failure. */
static int read_pgm(const unsigned amount,
                    unsigned char src[amount],
                    FILE *fin)
{
  /* Skip over the PGM header (P5\n16 12\n255\n). */
  int nl = 3;
  int c;
  while (nl > 0 && (c = getc(fin)) != EOF)
    if (c == '\n') nl--;

  /* Read the raw bytes. */
  size_t rrc = fread(src, amount, 1, fin);
  fclose(fin);
  return rrc == 0;
}

/* Compute the difference per pixel between two sets of images,
   'sum_after' (the most recent 'nf_after' frames) and 'sum_before'
   (the prior 'nf_before' frames).  The difference is diminished by
   high stddev in the prior frames, but amplified by high stddev in
   the most recent frames.  'vp_after' is the power used to convert
   variance to stddev for the most recent frames, and 'vp_before'
   likewise for prior frames.  These should be 0.5 for the normal
   meaning of stddev as standard deviation. */
static void compute_diffs(const unsigned width,
                          const unsigned height,
                          const unsigned nf_after,
                          const unsigned nf_before,
                          const double vp_after,
                          const double vp_before,
                          struct stats sum_after[height][width],
                          struct stats sum_before[height][width],
                          double diff[height][width])
{
  for (unsigned x = 0; x < width; x++) {
    for (unsigned y = 0; y < height; y++) {
        /* Compute means. */
        double mu1 = sum_after[y][x].sum / (double) nf_after;
        double mu0 = sum_before[y][x].sum / (double) nf_before;

        /* Compute standard deviations. */
        double var1 = sum_after[y][x].sum_sq / (double) nf_after - mu1 * mu1;
        double sig1 = pow(var1, vp_after);
        double var0 = sum_before[y][x].sum_sq / (double) nf_before - mu0 * mu0;
        double sig0 = pow(var0, vp_before);

        /* We care about the difference between the means, but we also
           want to give more significance to the difference if the
           'after' mean varies a lot, and less significance if the
           'before' mean varies a lot. */
        diff[y][x] = (mu1 - mu0) / 255.0;
        diff[y][x] *= (sig1 / 255.0 + 1.0) / (sig0 / 255.0 + 1.0);
      }
    }
}

static void sum_adjs(unsigned width, unsigned height,
                     struct vect motion[height][width],
                     double diff[height][width],
                     const void *zero)
{
  /* Reset the sums. */
  memcpy(motion, zero, sizeof motion[0][0] * width * height);

  /* Compute average vectors for each inner cell. */
  for (unsigned y = 1; y < height - 1; y++) {
    const unsigned up = y - 1;
    const unsigned down = y + 1;
    for (unsigned x = 1; x < width - 1; x++) {
      const unsigned left = x - 1;
      struct vect *const vp = &motion[y][x];

      {
        double mag = fdetedge(diff[y][x], diff[y][left]) / 8.0;
        vp->x += -1.0 * mag;
        motion[y][left].x += +1.0 * mag;
      }

      {
        double mag = fdetedge(diff[y][x], diff[up][x]) / 8.0;
        vp->y += -1.0 * mag;
        motion[up][x].y += +1.0 * mag;
      }

      {
        double mag = fdetedge(diff[y][x], diff[up][left]) / 8.0;
        vp->x += -ROOT2 * mag;
        vp->y += -ROOT2 * mag;
        motion[up][left].x += +ROOT2 * mag;
        motion[up][left].y += +ROOT2 * mag;
      }

      {
        double mag = fdetedge(diff[y][x], diff[down][left]) / 8.0;
        vp->x += -ROOT2 * mag;
        vp->y += +ROOT2 * mag;
        motion[down][left].x += +ROOT2 * mag;
        motion[down][left].y += -ROOT2 * mag;
      }
    }
  }
}


/* Read in 'hdeg' frames into 'src', using filenames from 'namelist'.
   Return 0 on success; -1 if the stream terminates. */
static int fill_up(const unsigned width,
                   const unsigned height,
                   const double fact,
                   const unsigned tdeg,
                   unsigned char src[][width][height],
                   size_t linebuflen,
                   char linebuf[linebuflen],
                   FILE *namelist)
{
  const char *srcname;
  const char *line;
  unsigned rplidx = 0;
  while (rplidx < tdeg) {
    if (!read_filenames(namelist, linebuflen, linebuf, &srcname, &line))
      return -1;
    if (*line == '\0') {
      /* No condensed file is actually being provided, but we must
         still report the original file. */
      report(0.0, 0.0, srcname, -1.0);
      continue;
    }

    /* Open the image file, and remove it. */
    FILE *fin = fopen(line, "rb");
    if (fin == NULL || read_pgm(width * height, &src[rplidx][0][0], fin) != 0) {
      report(0.0, 0.0, srcname, fact);
      continue;
    }

    report(0.0, 0.0, srcname, fact);
    rplidx++;
  }

  return 0;
}

int main(int argc, const char *const *argv)
{
  /* Filenames of images to process are read continuously from this
     stream. */
  const char *watch = NULL;

  /* Each image is expected to be an 8bpp PGM, of 16x12 pixels by
     default. */
  unsigned width = 16, height = 12;

  /* By default, 100 images in a sequence will be averaged to produce
     the 'before' image, and 5 for 'after'. */
  unsigned mdeg = 5;
  unsigned hdeg = 100;

  int factpow = 9;

  double varpow0 = 0.5, varpow1 = 0.5;

  /* Parse command-line arguments. */
  bool show_help = false, fail = false;
  for (int argi = 1; argi < argc; argi++) {
    if (!strcmp(argv[argi], "-f")) {
      if (++argi == argc) {
        show_help = true;
        fail = true;
        break;
      }
      watch = argv[argi];
    } else if (!strcmp(argv[argi], "+f")) {
      watch = NULL;
    } else if (!strcmp(argv[argi], "-p")) {
      if (++argi == argc) {
        show_help = true;
        fail = true;
        break;
      }
      factpow = atoi(argv[argi]);
    } else if (!strcmp(argv[argi], "-v")) {
      if (++argi == argc) {
        show_help = true;
        fail = true;
        break;
      }
      varpow0 = atof(argv[argi]);
    } else if (!strcmp(argv[argi], "-n")) {
      if (++argi == argc) {
        show_help = true;
        fail = true;
        break;
      }
      mdeg = atoi(argv[argi]);
    } else if (!strcmp(argv[argi], "-H")) {
      if (++argi == argc) {
        show_help = true;
        fail = true;
        break;
      }
      hdeg = atoi(argv[argi]);
    } else if (!strcmp(argv[argi], "-s")) {
      if (++argi == argc) {
        show_help = true;
        fail = true;
        break;
      }
      sscanf(argv[argi], "%ux%u", &width, &height);
    } else if (argv[argi][0] == '-' || argv[argi][0] == '+') {
      fprintf(stderr, "%s: unknown switch: %s\n", argv[0], argv[argi]);
      exit(EXIT_FAILURE);
    } else {
      fprintf(stderr, "%s: unknown argument: %s\n", argv[0], argv[argi]);
      exit(EXIT_FAILURE);
    }
  }

  if (show_help) {
    fprintf(stderr,
            "Usage: %s [-f file|+f]\n"
            "\t[-s WxH]\n"
            "\t[-n frames after]\n"
            "\t[-H frames before]\n"
            "\t[-v varpow]\n"
            "\t[-p power]\n", argv[0]);
    exit(fail ? EXIT_FAILURE : EXIT_SUCCESS);
  }

  const double fact = pow(10, factpow - 5);

  /* Create a zeroed structure that we can simply byte-copy to reset a
     similar structure. */
  struct vect zero_vect[height][width];
  for (unsigned i = 0; i < 2; i++) {
    for (unsigned y = 0; y < height; y++) {
      for (unsigned x = 0; x < width; x++) {
        zero_vect[y][x].x = 0.0;
        zero_vect[y][x].y = 0.0;
      }
    }
  }

  /* How many frames do we keep altogether?  It's the number of before
     and after frames. */
  const unsigned tdeg = mdeg + hdeg;

  /* Prepare to store the most recent mdeg images, and the previous
     hdeg images. */
  unsigned char src[tdeg][height][width];
  memset(src, 0, sizeof src);

  /* Store the sums for the 'before' and 'after' images.  We store
     both the sum and the sum of squares, so we can easily calculate
     the mean and standard deviation.  We must put y before x so that
     we can fread an image with a single call, as the data comes in
     row-by-row. */
  typedef struct stats sum_type[height][width];
  sum_type sum_before, sum_after;
  for (unsigned x = 0; x < width; x++)
    for (unsigned y = 0; y < height; y++)
      sum_before[y][x] = sum_after[y][x] = (struct stats) { 0 };

  FILE *namelist = watch == NULL ? stdin : fopen(watch, "r");
  if (namelist == NULL) {
    fprintf(stderr, "%s: could not read %s\n", argv[0], watch);
    exit(EXIT_FAILURE);
  }


  /* Read in the first tdeg images to fill our buffer. */
  char linebuf[PATH_MAX + 1];
  fprintf(stderr, "populating history\n");
  if (fill_up(width, height, fact, tdeg, src,
              sizeof linebuf, linebuf, namelist) == 0) {
    fprintf(stderr, "history complete\n");

    /* Make the frames contribute to their respective sums. */
    for (unsigned rplidx = 0; rplidx < hdeg; rplidx++)
      add_image(+1, width, height, sum_before, src[rplidx]);
    for (unsigned rplidx = hdeg; rplidx < tdeg; rplidx++)
      add_image(+1, width, height, sum_after, src[rplidx]);

    /* Compute the difference per pixel between the most recent frames
       and the prior frames. */
    double diff[height][width];
    compute_diffs(width, height, mdeg, hdeg, varpow1, varpow0,
                  sum_after, sum_before, diff);

    /* Record vector computations, old and new.  Initialize the new
       one [0] based on the initial 'diff'. */
    struct vect motion[2][height][width];
    unsigned next_mrplidx = 1;
    sum_adjs(width, height, motion[1 - next_mrplidx], diff, zero_vect);

    /* Read in additional frames. */
    const char *srcname;
    const char *line;
    unsigned sources = 0;
    while (read_filenames(namelist, sizeof linebuf, linebuf, &srcname, &line)) {
      if (*line == '\0') {
        /* No condensed file is actually being provided, but we must
           still report the original file. */
        report(0.0, 0.0, srcname, -1.0);
        continue;
      }

      /* Open the image file, and remove it. */
      FILE *fin = fopen(line, "rb");
      if (fin == NULL) {
        report(0.0, 0.0, srcname, fact);
        continue;
      }

      /* Which motion vector array are we replacing? */
      const unsigned mrplidx = next_mrplidx;
      const unsigned maltidx = next_mrplidx = 1 - mrplidx;

      /* Move images between the 'before' and 'after' sums.  'rplidx'
         holds the oldest frame, so that should be overwritten. */
      const unsigned rplidx = sources;

      /* Which image is to be subtracted from the 'after' sum and
         added to 'before'?  It's hdeg frames ahead, wrapped
         around. */
      const unsigned transidx = (rplidx + hdeg) % tdeg;

#if 0
      fprintf(stderr, "src %d; rpl %u; trans %u\n", sources, rplidx, transidx);
#endif

      /* Read in the PGM at rplidx.  If we fail, we don't count it. */
      unsigned char tmp[height][width];
      if (read_pgm(width * height, &tmp[0][0], fin) != 0) {
        memset(&src[rplidx][0][0], 0, width * height);
        report(0.0, 0.0, srcname, fact);
        continue;
      }

      /* We got a complete image, so ensure we move to the next frame
         in our buffer. */
      if (++sources == tdeg)
        sources = 0;

      /* Subtract the old image from the 'before' sum. */
      add_image(-1, width, height, sum_before, src[rplidx]);

      /* Move the middle image from 'after' to 'before'. */
      add_image(-1, width, height, sum_after, src[transidx]);
      add_image(+1, width, height, sum_before, src[transidx]);

      /* Copy the new image into place. */
      memcpy(src[rplidx], tmp, sizeof tmp);

      /* Add the new image to the after sum. */
      add_image(+1, width, height, sum_after, src[rplidx]);


      /* Start by computing the differences in the means of each pixel,
         and the ratio of standard deviations. */
      double diff[height][width];
      compute_diffs(width, height, mdeg, hdeg, varpow1, varpow0,
                    sum_after, sum_before, diff);

#if 0
      for (unsigned y = 0; y < height; y++) {
        for (unsigned i = 0; i < mdeg + 1; i++) {
          for (unsigned x = 0; x < width; x++) {
            fprintf(stderr, "%d",
                    (int) (src[(rplidx + i + hdeg - 1) %
                               tdeg][y][x] * 10.0 / 255));
          }
          putc(' ', stderr);
        }
        for (unsigned x = 0; x < width; x++) {
          fprintf(stderr, "%d",
                  (int) (sum_before[y][x].sum * 10.0 / (hdeg * 255  + 1)));
        }
        putc(' ', stderr);
        for (unsigned x = 0; x < width; x++) {
          fprintf(stderr, "%d",
                  (int) (sum_after[y][x].sum * 10.0 / (mdeg * 255 + 1)));
        }
        putc(' ', stderr);
        for (unsigned x = 0; x < width; x++) {
          int sn = copysign(1.0, diff[y][x]);
          fprintf(stderr, "%s", sn < 0 ? "-" : sn > 0 ? "+" : "0");
        }
        putc('\n', stderr);
      }
#endif


      /* Compare each pixel difference with each of its neighbours. */
      sum_adjs(width, height, motion[mrplidx], diff, zero_vect);

#if 0
      for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x++) {
          struct vect *const vp = &motion[mrplidx][y][x];
          fprintf(stderr, "(%6.3f %6.3f) ", vp->x * 1e2, vp->y * 1e2);
        }
        fprintf(stderr, "\n");
      }
#endif

      /* Compare previous (maltidx) and current (mrplidx) vectors for
         inner cells. */
      double sum = 0.0, sum2 = 0.0;
      for (unsigned y = 1; y < height - 1; y++) {
        const unsigned up = y - 1;
        const unsigned down = y + 1;
        for (unsigned x = 1; x < width; x++) {
          const unsigned left = x - 1;
          struct vect *const vp = &motion[mrplidx][y][x];
          double s = 0.0;
          s += cos2vect(vp, &motion[maltidx][up][x]);
          s += cos2vect(vp, &motion[maltidx][up][left]);
          s += cos2vect(vp, &motion[maltidx][y][left]);
          s += cos2vect(vp, &motion[maltidx][down][left]);
          sum += s;
          sum2 += s * s;
        }
      }
      const double mean = sum / ((width - 1) * (height - 1));
      const double var = sum2 / ((width - 1) * (height - 1)) - mean * mean;
      const double sd = sqrt(var);
      report(mean, sd, srcname, fact);
    }
  }

  if (namelist != stdin)
    fclose(namelist);

  return 0;
}
