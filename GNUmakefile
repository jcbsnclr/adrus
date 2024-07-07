# adrus - simple CLI note manager
# Copyright (C) 2024  Jacob Sinclair <jcbsnclr@outlook.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

CC?=gcc
DBG?=gdb

TARGET?=debug

CSRC_BIN:=$(wildcard adrus/*.c)
COBJ_BIN:=$(patsubst adrus/%.c, build/adrus/%.c.o, $(CSRC_BIN))

CSRC_LIB:=$(wildcard jbase/*.c)
COBJ_LIB:=$(patsubst jbase/%.c, build/jbase/%.c.o, $(CSRC_LIB))

CFLAGS+=-Wall -Wextra  -Werror -c -MMD
LFLAGS+=-lm

ifeq ($(TARGET), debug)
CFLAGS+=-Og -g -fsanitize=undefined -fstack-protector-strong -DJBASE_ASSERT
LFLAGS+=-fsanitize=undefined -fstack-protector-strong
else ifeq ($(TARGET), release)
CFLAGS+=-O2 
else
$(error "unknown target $(TARGET)")
endif

DEPS:=

CFLAGS+=$(foreach dep, $(DEPS), $(shell pkg-config --cflags $(dep)))
LFLAGS+=$(foreach dep, $(DEPS), $(shell pkg-config --libs $(dep)))

CFLAGS_BIN:=-Iadrus/ -Idist/
CFLAGS_LIB:=-Ijbase/ -Idist/ 

BIN:=build/adrus/adrus
LIB:=build/jbase/libjbase.a

build/jbase/%.c.o: jbase/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CFLAGS_LIB) $< -o $@

$(LIB): $(COBJ_LIB)
	mkdir -p $(dir $@)
	ar -cvq $@ $(COBJ_LIB)

build/adrus/%.c.o: adrus/%.c $(LIB)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CFLAGS_BIN) $< -o $@

$(BIN): $(COBJ_BIN) $(LIB)
	$(CC) $(LFLAGS) $(COBJ_BIN) $(LIB)  -o $@

.PHONY: all lib base run debug clean

all: $(BIN) $(LIB)

run: $(BIN) $(LIB)
	LOG_FILTER=trace ./$(BIN) 

debug: $(BIN) $(LIB)
	$(DBG) $(BIN)

clean: 
	rm -rf build/

-include build/adrus/*.c.d 
-include build/jbase/*.c.d
