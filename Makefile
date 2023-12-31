## Copyright 2018-19, Lancaster University
## All rights reserved.
## 
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions are
## met:
## 
##  * Redistributions of source code must retain the above copyright
##    notice, this list of conditions and the following disclaimer.
## 
##  * Redistributions in binary form must reproduce the above copyright
##    notice, this list of conditions and the following disclaimer in the
##    documentation and/or other materials provided with the
##    distribution.
## 
##  * Neither the name of the copyright holder nor the names of
##    its contributors may be used to endorse or promote products derived
##    from this software without specific prior written permission.
## 
## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
## "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
## LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
## A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
## OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
## SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
## LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
## DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
## THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
## (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
## OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
##
##
## Author: Steven Simpson <https://github.com/simpsonst>

all::

PREFIX=/usr/local
INSTALL=install
FIND=find
TOUCH=touch
MKDIR=mkdir -p
CHOWN=chown
M4=m4
SED=sed
XARGS=xargs

## Provide a version of $(abspath) that can cope with spaces in the
## current directory.
myblank:=
myspace:=$(myblank) $(myblank)
MYCURDIR:=$(subst $(myspace),\$(myspace),$(CURDIR)/)
MYABSPATH=$(foreach f,$1,$(if $(patsubst /%,,$f),$(MYCURDIR)$f,$f))

-include $(call MYABSPATH,config.mk)
-include stecam-env.mk

BINODEPS_SCRIPTDIR=src/share
LIBEXECDIR=$(PREFIX)/libexec/stecam
SHAREDIR=$(PREFIX)/share/stecam
SBINDIR=$(PREFIX)/sbin

datafiles += defaults.sh
datafiles += common.sh

hidden_scripts += stecamd
hidden_scripts += mpmr
admin_scripts += stecam-capture
scripts += stecam-serve

hidden_binaries.c += modect
modect_obj += modect
modect_lib += -lm

hidden_binaries.c += stecam-serve-bin
stecam-serve-bin_obj += serve

include binodeps.mk

out/stecam@.service: src/stecam.service.m4
	$(MKDIR) "$(@D)"
	$(M4) -DSHAREDIR="$(SHAREDIR)" -DSBINDIR="$(SBINDIR)" "$<" > "$@"

all:: installed-binaries out/stecam@.service

install-apt::
	apt-get install \
	    bc \
	    m4 \
	    par \
	    imagemagick \
	    inotify-tools \
	    ffmpeg \
	    libjpeg-progs \
	    streamer

install-systemd::
	$(INSTALL) -m 755 -d /etc/stecam.d
	$(INSTALL) -m 644 out/stecam@.service /etc/systemd/system/
	systemctl daemon-reload

install:: install-data
install:: install-hidden-binaries
install:: install-hidden-scripts
install:: install-admin-scripts
install:: install-scripts

tidy::
	@$(FIND) . -name "*~" -delete

blank:: clean

YEARS=2018-19

update-licence:
	$(FIND) . -name '.git' -prune -or -type f -print0 | $(XARGS) -0 \
	$(SED) -i 's/Copyright\s\+[0-9,]\+\sLancaster University/Copyright $(YEARS), Lancaster University/g'
