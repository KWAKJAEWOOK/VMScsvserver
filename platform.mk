.SUFFIXES : .c .cpp .o

.cpp.o:
	$(CCC) $(CCFLAGS) -c $(IPATHS) $<

.c.o:
	$(CC) $(CFLAGS) -c $(IPATHS) $<

.y.c:

%.o : $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $(IPATHS) $<

# Platform specific definitions for Linux/GNU.

# The following variables are platform specific and may need
# to be edited if moving to a different system.

# compiler defs
CC = gcc
CCC = g++
CFLAGS_   = $(GCCWARN_) -Wnested-externs -std=gnu11
CCFLAGS_  = $(GCCWARN_) -Wno-reorder $(CEXCEPT_) -std=gnu++11
CVARS_    = -DASN1RT -DGNU -D_GNU_SOURCE -DHAVE_STDINT_H -DHAVE_VA_COPY -fPIC
LINKDBG_  = -g
CELF_     = -fPIC
CUSEELF_  = -fpic
CEXCEPT_  = -fexceptions -fpermissive
CVARS0_   = $(CVARS_)
CVARSR_   = $(CVARS_)
CVARSMT_  = $(CVARS_)
CVARSMTR_ = $(CVARS_)
CVARSMTD_ = $(CVARS_)
CVARSMD_  = $(CVARS_)
CVARSMDR_ = $(CVARS_)
CVARSMDD_ = $(CVARS_)
CVARSMTR  = $(CVARS_)
COPTIMIZE0_ = -O3
GCCWARN_ = -Wall -Wpointer-arith -Wextra -Wundef -Wno-unused-parameter -Wshadow -Wcast-align -Wcomments -Wredundant-decls
WERROR = -Werror
LINKOPTR_ = -o $@ -Wl,-Bstatic
LINKOPTD_ = $(LINKDBG_) $(LINKOPTR_)
COPTIMIZE_ = $(COPTIMIZE0_) -D_OPTIMIZED
CSPACEOPT_ = -Os
CTIMEOPT_ = -O3

CPP11 = -std=c++11

BLDSUBDIR  = release
CDEV_      = -D_TRACE -g
CDEBUG_    = -$(CDEV_)
CBLDTYPE_ = $(COPTIMIZE_)
LINKOPT_ = -o $@ -Wl,-Bstatic
LINKOPTRLM_ = $(LINKOPT_)
CBLDTYPE_  = $(COPTIMIZE_)


CDEBUG_   = $(CDEV_)
CCDEBUG_  = $(CDEBUG_)

LIBCMD    = ar r $@
LIBADD    = $(LIBCMD)
LINK = gcc
LINKSO    = $(LINK)
LINKOPT2  = $(LINKOPT_)
LINKELF_  = -shared $(CELF_) -o $@
LINKELF2_ = $(LINKELF_)
LINKUELF_ = $(CUSEELF_) -o $@
LINKOPTDYN_ = $(LINKUELF_)
LINKOPT2D   = $(LINKDBG_)
LINKDLLOPTD = $(LINKDBG_)
COMPACT   = -Os -D_COMPACT
OBJOUT    = -o $@
OUTFILE   = -o

# File extensions
EXE     = 
OBJ     = .o
SO      = .so

# Run-time library
LIBPFX  = lib
LIBEXT  = a
LPPFX   = -L
LLPFX   = -l
LLEXT   =
LLAEXT  = 

A       = .$(LIBEXT)
MTA     = $(A)
MDA     = $(A)
MD      =
IMP     = $(SO)
IMPEXT  = $(IMP)
IMPLINK =
DLL     = $(SO)
RTDIRSFX =

# Include and library paths
PS      = /
FS      = :
IPATHS_ = 
CSC     = dmcs

# O/S commands
COPY     = cp -f
MOVE     = mv -f
MV       = $(MOVE)
RM       = rm -f
STRIP    = strip
MAKE     = make
RMDIR    = rm -rf
MKDIR    = mkdir -p

LLSYS = -Wl,-Bdynamic -lm -lpthread -ldl

# Sample libraries
EMPLOYEE_LIB =
EMPLOYEE_LL  = $(LLPFX)Employee

# LIBXML2 defs
LIBXML2ROOT = $(OSROOTDIR)/libxml2src
LIBXML2INC  = $(LIBXML2ROOT)/include
LIBXML2LIBDIR = ../lib
LIBXML2NAME = libxml2.a
LIBXML2LINK = -lxml2

# Python
PYTHON = python3

# RLM defs
RLMDIR = $(OSROOTDIR)/licmgr/RLM
RLMLIBDIR = $(RLMDIR)/bin

# Directories
CDIR    = c
CPPDIR  = cpp

# START ASN1C
# Link libraries
LLASN1RT3GPP = -lasn1rt3gpp
LLBER   = -lasn1ber
LLCBOR	= -losrtcbor
LLJSON  = -lasn1json
LLMDER   = -lasn1mder
LLOER   = -lasn1oer
LLPER   = -lasn1per
LLXML   = -lasn1xml
LLRT    = -lasn1rt
LLRLM   = -lrlm
LLLIC   = -llicense
LLMBEDTLS = -lmbedtls
LLASN1C = -lasn1c
LLOSCOM = -loscom
LLX2A   = -lxsd2asn1
LLX2AAC = -lxsd2asn1ac

LLASN1RT3GPPMD = -lasn1rt3gpp
LLASN1UTIL = -lasn1util
LLBERMD   = -lasn1ber
LLCBORMD  = -losrtcbor
LLJSONMD  = -lasn1json
LLMDERMD  = -lasn1mder
LLOERMD   = -lasn1oer
LLPERMD   = -lasn1per
LLPERIMPD = $(LLPERMD)
LLXMLMD   = -lasn1xml
LLXMLIMPD = $(LLXMLMD)
LLRTMD    = -lasn1rt
LLRTIMPD  = $(LLRTMD)
LLRLMMD   = -lrlm
LLASN1CMD = -lasn1c
LLOSCOMMD = -loscom
LLX2AMD   = -lxsd2asn1
LLX2AACMD = -lxsd2asn1ac

# library file names
A3GPPLIBNAME = libasn1rt3gpp.a
AUTILLIBNAME = libasn1util.a
RTLIBNAME =  libasn1rt.a
CBORLIBNAME = libosrtcbor.a
BERLIBNAME = libasn1ber.a
JSONLIBNAME = libasn1json.a
PERLIBNAME = libasn1per.a
XMLLIBNAME = libasn1xml.a
RLMLIBNAME = librlm.a
LICLIBNAME = liblicense.a
MBEDLIBNAME = libmbedtls.a
OSLIBNAME  = liboscom.a
X2ALIBNAME = libxsd2asn1.a
X2AACLIBNAME = libxsd2asn1ac.a

# these are needed for compatibility with the Windows version
RTLIBMDNAME =  libasn1rt.a
BERLIBMDNAME = libasn1ber.a
PERLIBMDNAME = libasn1per.a
XMLLIBMDNAME = libasn1xml.a
LIBXML2MDNAME = libxml2.a
LIBXML2MDLINK = -lxml2

# Run-time library sets used in ASN1C link
LLACXRT = $(LLXML) $(LLRT) $(LIBXML2LINK) $(LLSYS)
LLACLRT = $(LLBER) $(LLACXRT)
LLX2ART = $(LLPER) $(LLACLRT)

# END ASN1C

# START XBINDER
# XBinder specific platform definitions
# END XBINDER
