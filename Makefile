CC=g++
CFLAGS=-std=c++11 -Iinclude $(if $(DEBUG),-g,-O2) -Wno-write-strings
LFLAGS=-ldl -lboost_regex

EXE=$(EXEDIR)/genie
LIB_LUA=$(LIBDIR)/lua.a
LIB_LUASOCK=$(LIBDIR)/luasock.a
LIB_CORE=$(LIBDIR)/core.a
EXEDIR=bin
LIBDIR=lib

.PHONY: clean all

all: $(EXE)

clean:
	rm -f $(LIB_LUA) $(LUA_OBJS) \
		$(LIB_CORE) $(CORE_OBJS) \
		$(LIB_LUASOCK) $(LUASOCK_OBJS) \
		$(LPS_OBJS) \
		$(EXE) $(EXE_OBJS)

#
# LUA stuff
#

LUA_SRCDIRS=src/lua
LUA_CFILES=$(wildcard $(addsuffix /*.c*, $(LUA_SRCDIRS)))
LUA_HFILES=$(wildcard $(addsuffix /*.h, $(LUA_SRCDIRS)))
LUA_HFILES_PUB=$(wildcard include/genie/lua/*.h)
LUA_OBJS=$(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(LUA_CFILES)))

$(LUA_OBJS): $(LUA_CFILES) $(LUA_HFILES) $(LUA_HFILES_PUB)
	$(CC) $(CFLAGS) -DLUA_USE_LINUX -c $*.c* -o $@ -Isrc/lua

$(LIB_LUA): $(LUA_OBJS)
	@mkdir -p $(LIBDIR)
	ar rcs $(LIB_LUA) $(LUA_OBJS)
	
LUASOCK_SRCDIRS=src/luasocket
LUASOCK_CFILES=$(addprefix $(LUASOCK_SRCDIRS)/, auxiliar.c buffer.c compat.c except.c \
inet.c io.c luasocket.c mime.c options.c select.c serial.c tcp.c timeout.c udp.c \
unix.c unixtcp.c unixudp.c usocket.c)
LUASOCK_HFILES=$(wildcard $(addsuffix /*.h, $(LUASOCK_SRCDIRS)))
LUASOCK_OBJS=$(patsubst %.c,%.o,$(LUASOCK_CFILES))

$(LUASOCK_OBJS): $(LUASOCK_CFILES) $(LUASOCK_HFILES)
	$(CC) $(CFLAGS) -c $*.c -o $@ -Isrc/lua -DLUASOCKET_API="extern \"C\""

$(LIB_LUASOCK): $(LUASOCK_OBJS)
	@mkdir -p $(LIBDIR)
	ar rcs $(LIB_LUASOCK) $(LUASOCK_OBJS)
#
# lp_solve files
#

LPS_SRCDIRS=src/lp_solve
LPS_CFILES=$(addprefix $(LPS_SRCDIRS)/, colamd.c commonlib.c ini.c lp_crash.c lp_Hash.c \
lp_lib.c lp_LUSOL.c lp_matrix.c lp_MDO.c lp_mipbb.c lp_MPS.c lp_params.c lp_presolve.c lp_price.c \
lp_pricePSE.c lp_report.c lp_scale.c lp_simplex.c lp_SOS.c lp_utils.c lp_wlp.c lusol.c mmio.c myblas.c)
LPS_HFILES=$(wildcard $(addsuffix /*.h, $(LPS_SRCDIRS)))
LPS_OBJS=$(patsubst %.c,%.o,$(LPS_CFILES))

$(LPS_OBJS): $(LPS_CFILES) $(LPS_HFILES)
	$(CC) $(CFLAGS) -c $*.c -o $@ 

# Core library

CORE_SRCDIRS=src/core
CORE_CFILES=$(wildcard $(addsuffix /*.cpp, $(CORE_SRCDIRS)))
CORE_HFILES=$(wildcard $(addsuffix /*.h, $(CORE_SRCDIRS)))
CORE_HFILES_PUB=$(wildcard include/genie/*.h)
CORE_OBJS=$(patsubst %.cpp,%.o,$(CORE_CFILES))

$(CORE_OBJS): %.o : %.cpp $(CORE_HFILES) $(CORE_HFILES_PUB)
	$(CC) $(CFLAGS) -Isrc/lp_solve -c $*.cpp -o $@ 

$(LIB_CORE): $(CORE_OBJS) $(LPS_OBJS)
	@mkdir -p $(LIBDIR)
	ar rcs $(LIB_CORE) $(CORE_OBJS) $(LPS_OBJS)

# Main executable

EXE_SRCDIRS=src/main src/main/getoptpp
EXE_CFILES=$(wildcard $(addsuffix /*.cpp, $(EXE_SRCDIRS)))
EXE_HFILES=$(wildcard $(addsuffix /*.h, $(EXE_SRCDIRS)))
EXE_OBJS=$(patsubst %.cpp,%.o,$(EXE_CFILES))
EXE_LIBS=$(LIB_LUA) $(LIB_CORE) $(LIB_LUASOCK)

$(EXE_OBJS): %.o : %.cpp $(EXE_HFILES) $(CORE_HFILES_PUB)
	$(CC) $(CFLAGS) -c $< -o $@ -Isrc/luasocket -Isrc/lua \
	-DLUASOCKET_API="extern \"C\""

$(EXE): $(EXE_OBJS) $(EXE_LIBS)
	$(CC) $(CFLAGS) -o $(EXE) $(EXE_OBJS) $(EXE_LIBS) $(LFLAGS)

