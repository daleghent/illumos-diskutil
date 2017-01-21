#
# This file and its contents are supplied under the terms of the
# Common Development and Distribution License ("CDDL"), version 1.0.
# You may only use this file in accordance with the terms of version
# 1.0 of the CDDL.
#
# A full copy of the text of the CDDL should have accompanied this
# source.  A copy of the CDDL is also available via the Internet at
# http://www.illumos.org/license/CDDL.
#

#
# Copyright 2017 Joyent, Inc.
#

all: diskutil diskutil.64

MACH64 = amd64

CFLAGS = \
	-I/ws/plat/proto/usr/include \
	-I/ws/plat/projects/illumos/usr/src/uts/common \
	-I/ws/plat/projects/illumos/usr/src/lib/fm/topo/libtopo/common
LDFLAGS = -L /ws/plat/proto/usr/lib -L /ws/plat/proto/usr/lib/fm \
	  -R /usr/lib/fm \
	  -ltopo -lnvpair -lc -ldevinfo -ldevid -ldiskstatus -lcontract -lsysevent

LDFLAGS_64 = -L /ws/plat/proto/usr/lib/$(MACH64) -L /ws/plat/proto/usr/lib/fm/$(MACH64) \
	  -R /usr/lib/fm/$(MACH64) \
	  -ltopo -lnvpair -lc -ldevinfo -ldevid -ldiskstatus -lcontract -lsysevent

ILL_SRC=/ws/plat/projects/illumos/usr/src
DI_CFILES= \
	  $(ILL_SRC)/lib/fm/topo/libtopo/common/topo_list.c \
	  di_extra.c

diskutil: diskutil.c
	gcc -m32 $(CFLAGS) $(LDFLAGS) -o $@ $< $(DI_CFILES)

diskutil.64: diskutil.c
	gcc -m64 $(CFLAGS) $(LDFLAGS_64) -o $@ $< $(DI_CFILES)

.KEEP_STATE:
