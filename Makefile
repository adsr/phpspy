elfutils_subdirs:=lib libelf libebl libdwelf libdwfl libdw libcpu backends
elfutils_includes:=$(foreach subdir,$(elfutils_subdirs),-Ivendor/elfutils/$(subdir))

elfutils_libs:=libdw libelf libdwfl libebl libdwelf
elfutils_sources:=$(foreach lib,$(elfutils_libs),vendor/elfutils/$(lib)/$(lib).a)
elfutils_sources:=$(elfutils_sources) vendor/elfutils/lib/libeu.a
elfutils_sources:=$(elfutils_sources) vendor/elfutils/backends/libebl_x86_64_pic.a

phpspy_cflags:=-std=c99 -Wall -Wextra -pedantic -g -Ofast -pthread $(CFLAGS)
phpspy_libs:=$(LDLIBS)
phpspy_ldflags:=$(LDFLAGS)
phpspy_includes:=-I. -I./vendor
phpspy_defines:=-DUSE_TERMBOX=1
test_scripts:=$(wildcard tests/test_*.sh)
phpspy_sources:=phpspy.c pgrep.c top.c addr_libdw.c event_fout.c

prefix?=/usr/local

sinclude config.mk

has_pthread := $(shell $(LD) $(phpspy_ldflags) -lpthread -o/dev/null >/dev/null 2>&1 && echo :)
has_phpconf := $(shell command -v php-config                         >/dev/null 2>&1 && echo :)

$(or $(has_pthread), $(error Need libpthread))

ifdef USE_ZEND
  $(or $(has_phpconf), $(error Need php-config))
  phpspy_includes:=$(phpspy_includes) $$(php-config --includes)
  phpspy_defines:=$(phpspy_defines) -DUSE_ZEND=1
endif

libdw      := vendor/elfutils/libdw/libdw.a
libtermbox := vendor/termbox/build/src/libtermbox.a

all: phpspy_libdw

$(libdw):
	cd vendor/elfutils && autoreconf -if && ./configure --enable-maintainer-mode && $(MAKE) SUBDIRS="$(elfutils_subdirs)"

$(libtermbox):
	cd vendor/termbox && ./waf configure && ./waf --targets=termbox_static

update_deps:
	@git submodule update --init --depth 1

clean_deps:
	cd vendor/elfutils && make clean
	cd vendor/termbox  && ./waf clean

phpspy_libdw: update_deps $(libdw) $(libtermbox)
phpspy_libdw: phpspy_libs:=$(phpspy_libs) -ldl -lz -llzma -lbz2
phpspy_libdw: phpspy_includes:=$(phpspy_includes) $(elfutils_includes)
phpspy_libdw: phpspy_sources:=$(phpspy_sources) $(elfutils_sources) $(libtermbox)
phpspy_libdw: phpspy

phpspy: $(wildcard *.c *.h)
	@$(check)
	$(CC) $(phpspy_cflags) $(phpspy_includes) $(phpspy_defines) $(phpspy_sources) -o phpspy $(phpspy_ldflags) $(phpspy_libs)

test: phpspy $(test_scripts)
	@total=0; \
	pass=0; \
	for t in $(test_scripts); do \
		tput bold; echo TEST $$t; tput sgr0; \
		PHPSPY=./phpspy TEST_DIR=$$(dirname $$t) ./$$t; ec=$$?; echo; \
		[ $$ec -eq 0 ] && pass=$$((pass+1)); \
		total=$$((total+1)); \
	done; \
	printf "Passed %d out of %d tests\n" $$pass $$total ; \
	[ $$pass -eq $$total ] || exit 1

install: phpspy
	install -D -v -m 755 phpspy $(DESTDIR)$(prefix)/bin/phpspy

clean:
	rm -f phpspy

.PHONY: all phpspy_libdw test clean_deps update_deps install clean
