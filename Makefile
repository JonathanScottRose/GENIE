CC=g++
CFLAGS=-std=c++11 -Iinclude -Isrc/lua -Isrc/main -DLUA_USE_LINUX $(if $(DEBUG),-g,-O2) -Wno-write-strings
LFLAGS=-ldl -lboost_regex

EXE=$(EXEDIR)/genie
LIB_LUA=$(LIBDIR)/lua.a
LIB_CORE=$(LIBDIR)/core.a
EXEDIR=bin
LIBDIR=lib

.PHONY: clean all

all: $(EXE)

clean:
	@echo $(LUA_OBJS)
	rm -f $(LIB_LUA) $(LUA_OBJS) $(LIB_CORE) $(CORE_OBJS) $(EXE) $(EXE_OBJS)

#
# LUA stuff
#

LUA_SRCDIRS=src/lua
LUA_CFILES=$(wildcard $(addsuffix /*.c*, $(LUA_SRCDIRS)))
LUA_HFILES=$(wildcard $(addsuffix /*.h, $(LUA_SRCDIRS)))
LUA_HFILES_PUB=$(wildcard include/genie/lua/*.h)
LUA_OBJS=$(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(LUA_CFILES)))

$(LUA_OBJS): $(LUA_CFILES) $(LUA_HFILES) $(LUA_HFILES_PUB)
	$(CC) $(CFLAGS) -c $*.c* -o $@ -Iinclude/genie/lua

$(LIB_LUA): $(LUA_OBJS)
	@mkdir -p $(LIBDIR)
	ar rcs $(LIB_LUA) $(LUA_OBJS)

# Core library

LPSOLVE_CFILES=$(addprefix src/core/lp_solve/, colamd.c commonlib.c ini.c lp_crash.c lp_Hash.c \
lp_lib.c lp_LUSOL.c lp_matrix.c lp_MDO.c lp_mipbb.c lp_MPS.c lp_params.c lp_presolve.c lp_price.c \
lp_pricePSE.c lp_report.c lp_scale.c lp_simplex.c lp_SOS.c lp_utils.c lp_wlp.c lusol.c mmio.c myblas.c)

CORE_SRCDIRS=src/core
CORE_CFILES=$(wildcard $(addsuffix /*.c*, $(CORE_SRCDIRS))) $(LPSOLVE_CFILES)
CORE_HFILES=$(wildcard $(addsuffix /*.h, $(CORE_SRCDIRS)))
CORE_HFILES_PUB=$(wildcard include/genie/*.h)
CORE_OBJS=$(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(CORE_CFILES)))

$(CORE_OBJS): %.o : $(CORE_CFILES) $(CORE_HFILES) $(CORE_HFILES_PUB)
	$(CC) $(CFLAGS) -c $*.c* -o $@ 

$(LIB_CORE): $(CORE_OBJS)
	@mkdir -p $(LIBDIR)
	ar rcs $(LIB_CORE) $(CORE_OBJS)

# Main executable

EXE_SRCDIRS=src/main src/main/getoptpp
EXE_CFILES=$(wildcard $(addsuffix /*.cpp, $(EXE_SRCDIRS)))
EXE_HFILES=$(wildcard $(addsuffix /*.h, $(EXE_SRCDIRS)))
EXE_OBJS=$(patsubst %.cpp,%.o,$(EXE_CFILES))

$(EXE_OBJS): %.o : %.cpp $(EXE_HFILES) $(CORE_HFILES_PUB)
	$(CC) $(CFLAGS) -c $< -o $@

$(EXE): $(EXE_OBJS) $(LIB_LUA) $(LIB_CORE)
	$(CC) $(CFLAGS) -o $(EXE) $(EXE_OBJS) $(LIB_LUA) $(LIB_CORE) $(LFLAGS)

