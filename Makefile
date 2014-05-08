CC=g++
CFLAGS=-std=c++11 -Iinclude -Isrc/lua -Isrc/main -DLUA_USE_LINUX
LFLAGS=-ldl

EXE=$(EXEDIR)/connectool
LIB_LUA=$(LIBDIR)/lua.a
LIB_CT=$(LIBDIR)/core.a
EXEDIR=bin32
LIBDIR=lib32

.PHONY: clean all

all: $(EXE)

clean:
	rm -f $(LIB_LUA) $(LUA_OBJS) $(LIB_CT) $(CT_OBJS) $(EXE) $(EXE_OBJS)

#
# LUA stuff
#

LUA_SRCDIRS=src/lua
LUA_CFILES=$(wildcard $(patsubst %,%/*.c,$(LUA_SRCDIRS)))
LUA_HFILES=$(wildcard $(patsubst %,%/*.h,$(LUA_SRCDIRS)))
LUA_OBJS=$(patsubst %.c,%.o,$(LUA_CFILES))

$(LUA_OBJS): %.o : %.c $(LUA_HFILES)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB_LUA): $(LUA_OBJS)
	@mkdir -p $(LIBDIR)
	ar rcs $(LIB_LUA) $(LUA_OBJS)

# Core CT library

CT_SRCDIRS=src
CT_CFILES=$(wildcard $(patsubst %,%/*.cpp,$(CT_SRCDIRS)))
CT_HFILES=$(wildcard $(patsubst %,%/*.h,$(CT_SRCDIRS)))
CT_OBJS=$(patsubst %.cpp,%.o,$(CT_CFILES))

$(CT_OBJS): %.o : %.cpp $(CT_HFILES)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB_CT): $(CT_OBJS)
	@mkdir -p $(LIBDIR)
	ar rcs $(LIB_CT) $(CT_OBJS)

# Main executable

EXE_SRCDIRS=src/main src/main/getoptpp src/main/impl
EXE_CFILES=$(wildcard $(patsubst %,%/*.cpp,$(EXE_SRCDIRS)))
EXE_HFILES=$(wildcard $(patsubst %,%/*.h,$(EXE_SRCDIRS)))
EXE_OBJS=$(patsubst %.cpp,%.o,$(EXE_CFILES))

$(EXE_OBJS): %.o : %.cpp $(EXE_HFILES)
	$(CC) $(CFLAGS) -c $< -o $@

$(EXE): $(EXE_OBJS) $(LIB_LUA) $(LIB_CT)
	$(CC) $(CFLAGS) -o $(EXE) $(EXE_OBJS) $(LIB_LUA) $(LIB_CT) $(LFLAGS)

