# makefile to build generated files

include platform.mk

# include directories
OSROOTDIR = $(HOME)/asn1c-v790
LIBDIR  = $(OSROOTDIR)$(PS)c$(RTDIRSFX)$(PS)lib
PRJLIBDIR = .$(PS)lib
PRJBINDIR = .$(PS)bin
PRJOBJDIR = .$(PS)obj
RTSRCDIR = $(OSROOTDIR)$(PS)rtsrc 
RTXSRCDIR = $(OSROOTDIR)$(PS)rtxsrc 
PERSRCDIR = $(OSROOTDIR)$(PS)rtpersrc 
SRCDIR = .
HFILESDIR = . 

# compiler defs
CFLAGS = -c $(CVARS_) $(CFLAGS_) $(CBLDTYPE_)
IPATHS = -I$(SRCDIR) -I$(OSROOTDIR) -I$(HFILESDIR) -I. -I$(RTSRCDIR) -I$(PERSRCDIR) $(IPATHS_) 
LINKOPT = $(LINKOPT_)

# run-time libraries
LIBS = $(LIBDIR)/$(LIBPFX)asn1per$(A) $(LIBDIR)/$(LIBPFX)asn1rt$(A)
LLIBS = $(LLPFX)asn1per$(LLAEXT) $(LLPFX)asn1rt$(LLAEXT) $(LLSYS)
LPATHS = $(LPPFX)$(LIBDIR)

# reader 실행 파일 빌드에 필요한 object 파일들
READEROBJ = $(PRJOBJDIR)$(PS)reader$(OBJ) \
            $(PRJOBJDIR)$(PS)VMSconnection_manager$(OBJ) \
			$(PRJOBJDIR)$(PS)VMScontroller$(OBJ) \
			$(PRJOBJDIR)$(PS)VMSprotocol$(OBJ) \
			$(PRJOBJDIR)$(PS)cJSON$(OBJ) \
			$(PRJOBJDIR)$(PS)sds_json_parser$(OBJ) \
			$(PRJOBJDIR)$(PS)scenario_manager$(OBJ) \
			$(PRJOBJDIR)$(PS)minIni$(OBJ)

all : $(PRJBINDIR)$(PS)reader$(EXE)

$(PRJBINDIR)/reader$(EXE) : $(READEROBJ) $(LIBS)
	$(LINK) $(READEROBJ) $(LINKOPT) $(LPATHS) $(LLIBS)

$(PRJOBJDIR)$(PS)reader$(OBJ) : \
    $(SRCDIR)$(PS)reader.c \
    $(SRCDIR)$(PS)VMSconnection_manager.h \
	$(SRCDIR)$(PS)VMScontroller.h \
	$(SRCDIR)$(PS)VMSprotocol.h \
	$(SRCDIR)$(PS)cJSON.h \
	$(SRCDIR)$(PS)sds_json_types.h
	$(CC) -c $(CFLAGS) $(OBJOUT) $(IPATHS) $(SRCDIR)$(PS)reader.c

$(PRJOBJDIR)$(PS)VMSconnection_manager$(OBJ) : $(SRCDIR)$(PS)VMSconnection_manager.c $(SRCDIR)$(PS)VMSconnection_manager.h $(SRCDIR)$(PS)minIni.h
	$(CC) $(CFLAGS) $(OBJOUT) -c $(IPATHS) $(SRCDIR)$(PS)VMSconnection_manager.c

# VMScontroller 오브젝트 빌드 규칙 추가
$(PRJOBJDIR)$(PS)VMScontroller$(OBJ) : $(SRCDIR)$(PS)VMScontroller.c $(SRCDIR)$(PS)VMScontroller.h $(SRCDIR)$(PS)sds_json_types.h $(SRCDIR)$(PS)minIni.h
	$(CC) $(CFLAGS) $(OBJOUT) -c $(IPATHS) $(SRCDIR)$(PS)VMScontroller.c

$(PRJOBJDIR)$(PS)VMSprotocol$(OBJ) : $(SRCDIR)$(PS)VMSprotocol.c $(SRCDIR)$(PS)VMSprotocol.h
	$(CC) $(CFLAGS) $(OBJOUT) -c $(IPATHS) $(SRCDIR)$(PS)VMSprotocol.c

$(PRJOBJDIR)$(PS)cJSON$(OBJ) : $(SRCDIR)$(PS)cJSON.c $(SRCDIR)$(PS)cJSON.h
	$(CC) $(CFLAGS) $(OBJOUT) -c $(IPATHS) $(SRCDIR)$(PS)cJSON.c

$(PRJOBJDIR)$(PS)sds_json_parser$(OBJ) : $(SRCDIR)$(PS)sds_json_parser.c $(SRCDIR)$(PS)sds_json_types.h $(SRCDIR)$(PS)cJSON.h
	$(CC) $(CFLAGS) $(OBJOUT) -c $(IPATHS) $(SRCDIR)$(PS)sds_json_parser.c

$(PRJOBJDIR)$(PS)scenario_manager$(OBJ) : $(SRCDIR)$(PS)scenario_manager.c $(SRCDIR)$(PS)scenario_manager.h
	$(CC) $(CFLAGS) $(OBJOUT) -c $(IPATHS) $(SRCDIR)$(PS)scenario_manager.c

# minIni 오브젝트 빌드 규칙 추가
$(PRJOBJDIR)$(PS)minIni$(OBJ) : $(SRCDIR)$(PS)minIni.c $(SRCDIR)$(PS)minIni.h $(SRCDIR)$(PS)minGlue.h
	$(CC) $(CFLAGS) $(OBJOUT) -c $(IPATHS) $(SRCDIR)$(PS)minIni.c

clean:
	$(RM) $(PRJOBJDIR)$(PS)*$(OBJ)
	$(RM) *~
	$(RM) $(PRJBINDIR)$(PS)reader$(EXE)
	$(RM_MSVC_FILES)
