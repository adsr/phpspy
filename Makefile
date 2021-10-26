phpspy_cflags:=-std=c11 -Wall -Wextra -pedantic -g -O3 -Wno-address-of-packed-member $(CFLAGS)
phpspy_libs:=-pthread $(LDLIBS)
phpspy_ldflags:=$(LDFLAGS)
phpspy_includes:=-I. -I./vendor
phpspy_defines:=
phpspy_tests:=$(wildcard tests/test_*.sh)
phpspy_sources:=phpspy.c pgrep.c top.c addr_objdump.c event_fout.c event_callgrind.c

termbox_inlcudes=-Ivendor/termbox2/

prefix?=/usr/local

php_path?=php

sinclude config.mk

has_phpconf := $(shell command -v php-config >/dev/null 2>&1 && echo :)

ifdef USE_ZEND
  $(or $(has_phpconf), $(error Need php-config))
  phpspy_includes:=$(phpspy_includes) $$(php-config --includes)
  phpspy_defines:=$(phpspy_defines) -DUSE_ZEND=1
endif

ifdef COMMIT
  phpspy_defines:=$(phpspy_defines) -DCOMMIT=$(COMMIT)
endif

all: phpspy

phpspy: $(wildcard *.c *.h) vendor/termbox2/termbox.h
	$(CC) $(phpspy_cflags) $(phpspy_includes) $(termbox_inlcudes) $(phpspy_defines) $(phpspy_sources) -o phpspy $(phpspy_ldflags) $(phpspy_libs)

vendor/termbox2/termbox.h:
	git submodule update --init --remote --recursive
	cd vendor/termbox2 && git reset --hard

test: phpspy $(phpspy_tests)
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

install: phpspy
	install -D -v -m 755 phpspy $(DESTDIR)$(prefix)/bin/phpspy

clean:
	cd vendor/termbox2 && $(MAKE) clean
	rm -f phpspy

.PHONY: all test install clean phpspy
