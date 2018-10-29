phpspy_cflags:=-std=c99 -Wall -Wextra -pedantic -g -Ofast -pthread $(CFLAGS)
phpspy_libs:=$(LDLIBS)
phpspy_ldflags:=$(LDFLAGS)
phpspy_includes:=-I. -I./vendor -Ivendor/termbox/src
phpspy_defines:=-DUSE_TERMBOX=1
phpspy_tests:=$(wildcard tests/test_*.sh)
phpspy_sources:=phpspy.c pgrep.c top.c addr_libdw.c event_fout.c

elfutils_subdirs:=lib libelf libebl libdwelf libdwfl libdw libcpu backends
elfutils_includes:=$(foreach subdir,$(elfutils_subdirs),-Ivendor/elfutils/$(subdir))
elfutils_includes:=$(elfutils_includes) -Ivendor/symlinks/libdwfl
elfutils_static_libs:=dw elf dwfl ebl dwelf
elfutils_ldflags:=$(foreach lib,$(elfutils_static_libs),-Lvendor/elfutils/lib$(lib))
elfutils_libs:=-Wl,-Bstatic
elfutils_libs:=$(elfutils_libs) $(foreach lib,$(elfutils_static_libs),-l$(lib))
elfutils_libs:=$(elfutils_libs) -Lvendor/elfutils/lib -leu
elfutils_libs:=$(elfutils_libs) -Lvendor/elfutils/backends -lebl_x86_64_pic
elfutils_libs:=$(elfutils_libs) -Wl,-Bdynamic
elfutils_libs:=$(elfutils_libs) -ldl -lz -llzma -lbz2

termbox_inlcudes=-Ivendor/termbox/src/
termbox_libs:=-Wl,-Bstatic -Lvendor/termbox/build/src/ -ltermbox -Wl,-Bdynamic

prefix?=/usr/local

php_path?=php

sinclude config.mk

has_pthread := $(shell $(LD) $(phpspy_ldflags) -lpthread -o/dev/null >/dev/null 2>&1 && echo :)
has_libdw   := $(shell $(LD) $(phpspy_ldflags) -ldw      -o/dev/null >/dev/null 2>&1 && echo :)
has_termbox := $(shell $(LD) $(phpspy_ldflags) -ltermbox -o/dev/null >/dev/null 2>&1 && echo :)
has_phpconf := $(shell command -v php-config                         >/dev/null 2>&1 && echo :)

$(or $(has_pthread), $(error Need libpthread))

ifdef USE_ZEND
  $(or $(has_phpconf), $(error Need php-config))
  phpspy_includes:=$(phpspy_includes) $$(php-config --includes)
  phpspy_defines:=$(phpspy_defines) -DUSE_ZEND=1
endif

all: phpspy_static

phpspy_static: $(wildcard *.c *.h) vendor/elfutils/libdw/libdw.a vendor/termbox/build/src/libtermbox.a
	$(CC) $(phpspy_cflags) $(phpspy_includes) $(elfutils_includes) $(termbox_inlcudes) $(phpspy_defines) $(phpspy_sources) -o phpspy $(phpspy_ldflags) $(elfutils_ldflags) $(phpspy_libs) $(elfutils_libs) $(termbox_libs)

phpspy_dynamic: $(wildcard *.c *.h)
	@$(or $(has_libdw), $(error Need libdw. Hint: try `make phpspy_static`))
	@$(or $(has_termbox), $(error Need libtermbox. Hint: try `make phpspy_static`))
	$(CC) $(phpspy_cflags) $(phpspy_includes) $(phpspy_defines) $(phpspy_sources) -o phpspy $(phpspy_ldflags) $(phpspy_libs) -ldw -ltermbox

vendor/elfutils/libdw/libdw.a: vendor/elfutils/configure.ac
	cd vendor/elfutils && autoreconf -if && ./configure --enable-maintainer-mode && $(MAKE) SUBDIRS="$(elfutils_subdirs)"

vendor/termbox/build/src/libtermbox.a: vendor/termbox/waf
	cd vendor/termbox && ./waf configure && ./waf --targets=termbox_static

vendor/termbox/waf:
	git submodule update --init --remote --recursive
	cd vendor/termbox && git reset --hard

vendor/elfutils/configure.ac:
	git submodule update --init --remote --recursive
	cd vendor/elfutils && git reset --hard

test: phpspy_static $(phpspy_tests)
	@total=0; \
	pass=0; \
	for t in $(phpspy_tests); do \
		tput bold; echo TEST $$t; tput sgr0; \
		PHPSPY=./phpspy PHP=$(php_path) TEST_SH=$$(dirname $$t)/test.sh ./$$t; ec=$$?; echo; \
		[ $$ec -eq 0 ] && pass=$$((pass+1)); \
		total=$$((total+1)); \
	done; \
	printf "Passed %d out of %d tests\n" $$pass $$total ; \
	[ $$pass -eq $$total ] || exit 1

install: phpspy_static
	install -D -v -m 755 phpspy $(DESTDIR)$(prefix)/bin/phpspy

clean:
	cd vendor/elfutils && make clean
	cd vendor/termbox && ./waf clean
	rm -f phpspy

.PHONY: all test install clean phpspy_static phpspy_dynamic
