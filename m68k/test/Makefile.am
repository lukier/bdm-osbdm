## 
## $Id: Makefile.am,v 1.4 2008/03/06 10:35:40 cjohns Exp $
##
## This file is part of a free BDM package
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##

AM_CPPFLAGS = -I$(srcdir)/../driver -I$(srcdir)/../lib

##
## Only way I know of anding in automake.
##

if WIN32
if BDM_REMOTE
LIBS += -l wsock32
endif
endif

if LIB_BDMUSB
BDMUSB_LIB = $(top_builddir)/libbdmusb/libbdmusb.a
endif

if BDM_USB
AM_CPPFLAGS += -I@LIBUSB_INCLUDE_DIR@
AM_LDFLAGS = -L@LIBUSB_LIB_DIR@
LIBS += -l@LIBUSB_LIB_NAME@
endif

bin_PROGRAMS = bdm-chk bdm-cpu32-chk

bdm_chk_SOURCES = \
	chk.c
bdm_chk_LDADD = ../lib/libBDM.a $(BDMUSB_LIB)

bdm_cpu32_chk_SOURCES = \
	cpu32-check.c
bdm_cpu32_chk_LDADD = ../lib/libBDM.a $(BDMUSB_LIB)
