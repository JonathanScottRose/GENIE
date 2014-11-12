CC=g++
CFLAGS=-std=c++11 -Iinclude -Isrc/lua -Isrc/main -DLUA_USE_LINUX -g
LFLAGS=-ldl -lboost_regex

EXE=$(EXEDIR)/genie
LIB_LUA=$(LIBDIR)/lua.a
LIB_CT=$(LIBDIR)/core.a
EXEDIR=bin
LIBDIR=lib

.PHONY: clean all

all: $(EXE)

clean:
	rm -f $(LIB_LUA) $(LUA_OBJS) $(LIB_CT) $(CT_OBJS) $(EXE) $(EXE_OBJS)

#
# LUA stuff
#

LUA_SRCDIRS=src/lua
LUA_CFILES=$(wildcard $(addsuffix /*.c*, $(LUA_SRCDIRS)))
LUA_HFILES=$(wildcard $(addsuffix /*.h, $(LUA_SRCDIRS)))
LUA_OBJS=$(patsubst %.c*,%.o,$(LUA_CFILES))

$(LUA_OBJS): %.o : %.c* $(LUA_HFILES)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB_LUA): $(LUA_OBJS)
	@echo $(LUA_CFILES)
	@mkdir -p $(LIBDIR)
	ar rcs $(LIB_LUA) $(LUA_OBJS)

# Core library

CT_SRCDIRS=src
CT_CFILES=$(wildcard $(addsuffix /*.cpp, $(CT_SRCDIRS)))
CT_HFILES=$(wildcard $(addsuffix /*.h, $(CT_SRCDIRS)))
CT_HFILES_PUB=$(wildcard include/ct/*.h)
CT_OBJS=$(patsubst %.cpp,%.o,$(CT_CFILES))

$(CT_OBJS): %.o : %.cpp $(CT_HFILES) $(CT_HFILES_PUB)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB_CT): $(CT_OBJS)
	@mkdir -p $(LIBDIR)
	ar rcs $(LIB_CT) $(CT_OBJS)

# Main executable

EXE_SRCDIRS=src/main src/main/getoptpp src/main/impl
EXE_CFILES=$(wildcard $(addsuffix /*.cpp, $(EXE_SRCDIRS)))
EXE_HFILES=$(wildcard $(addsuffix /*.h, $(EXE_SRCDIRS)))
EXE_OBJS=$(patsubst %.cpp,%.o,$(EXE_CFILES))

$(EXE_OBJS): %.o : %.cpp $(EXE_HFILES) $(CT_HFILES_PUB)
	$(CC) $(CFLAGS) -c $< -o $@

$(EXE): $(EXE_OBJS) $(LIB_LUA) $(LIB_CT)
	$(CC) $(CFLAGS) -o $(EXE) $(EXE_OBJS) $(LIB_LUA) $(LIB_CT) $(LFLAGS)

