phpspy_cflags:=-std=c90 -Wall -Wextra -pedantic -g -O3 $(CFLAGS)
phpspy_libs:=-pthread $(LDLIBS)
phpspy_ldflags:=$(LDFLAGS)
phpspy_includes:=-I. -I./vendor -Ivendor/termbox/src
phpspy_defines:=
phpspy_tests:=$(wildcard tests/test_*.sh)
phpspy_sources:=phpspy.c pgrep.c top.c addr_objdump.c event_fout.c event_callgrind.c

termbox_inlcudes=-Ivendor/termbox/src/
termbox_libs:=-Wl,-Bstatic -Lvendor/termbox/build/src/ -ltermbox -Wl,-Bdynamic

prefix?=/usr/local

php_path?=php

sinclude config.mk

has_pthread := $(shell $(LD) $(phpspy_ldflags) -lpthread -o/dev/null >/dev/null 2>&1 && echo :)
has_termbox := $(shell $(LD) $(phpspy_ldflags) -ltermbox -o/dev/null >/dev/null 2>&1 && echo :)
has_phpconf := $(shell command -v php-config                         >/dev/null 2>&1 && echo :)

$(or $(has_pthread), $(error Need libpthread))

ifdef USE_ZEND
  $(or $(has_phpconf), $(error Need php-config))
  phpspy_cflags:=$(subst c90,c11,$(phpspy_cflags))
  phpspy_includes:=$(phpspy_includes) $$(php-config --includes)
  phpspy_defines:=$(phpspy_defines) -DUSE_ZEND=1
endif

all: phpspy_static

phpspy_static: $(wildcard *.c *.h) vendor/termbox/build/src/libtermbox.a
	$(CC) $(phpspy_cflags) $(phpspy_includes) $(termbox_inlcudes) $(phpspy_defines) $(phpspy_sources) -o phpspy $(phpspy_ldflags) $(phpspy_libs) $(termbox_libs)

phpspy_dynamic: $(wildcard *.c *.h)
	@$(or $(has_termbox), $(error Need libtermbox. Hint: try `make phpspy_static`))
	$(CC) $(phpspy_cflags) $(phpspy_includes) $(phpspy_defines) $(phpspy_sources) -o phpspy $(phpspy_ldflags) $(phpspy_libs) -ltermbox

vendor/termbox/build/src/libtermbox.a: vendor/termbox/waf
	cd vendor/termbox && ./waf configure && ./waf --targets=termbox_static

vendor/termbox/waf:
	git submodule update --init --remote --recursive
	cd vendor/termbox && git reset --hard

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
	cd vendor/termbox && ./waf clean
	rm -f phpspy

.PHONY: all test install clean phpspy_static phpspy_dynamic
