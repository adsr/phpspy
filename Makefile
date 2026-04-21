phpspy_cflags:=-std=c11 -Wall -Wextra -pedantic -g -O3 -Wno-address-of-packed-member -DTB_LIB_OPTS $(CFLAGS)
phpspy_libs:=-pthread $(LDLIBS)
phpspy_ldflags:=$(LDFLAGS)
phpspy_includes:=-I. -I./vendor
phpspy_defines:=
phpspy_tests:=$(wildcard tests/test_*.sh)
phpspy_sources:=phpspy.c pgrep.c top.c addr_objdump.c event_fout.c event_callgrind.c

prefix?=/usr/local

php_cmd?=php -n

sinclude config.mk

has_phpconf := $(shell command -v php-config >/dev/null 2>&1 && echo :)
git_sha := $(shell test -d .git && command -v git >/dev/null && git rev-parse --short HEAD)

ifdef USE_ZEND
  $(or $(has_phpconf), $(error Need php-config))
  phpspy_includes:=$(phpspy_includes) $$(php-config --includes)
  phpspy_defines:=$(phpspy_defines) -DUSE_ZEND=1
endif

ifdef COMMIT
  phpspy_defines:=$(phpspy_defines) -DCOMMIT=$(COMMIT)
else ifneq ($(git_sha),)
  phpspy_defines:=$(phpspy_defines) -DCOMMIT=$(git_sha)
endif

all: phpspy

phpspy: $(wildcard *.c *.h)
	$(CC) $(phpspy_cflags) $(phpspy_includes) $(phpspy_defines) $(phpspy_sources) -o phpspy $(phpspy_ldflags) $(phpspy_libs)

test: phpspy $(phpspy_tests)
	@total=0; \
	pass=0; \
	for t in $(phpspy_tests); do \
		tput bold; echo TEST $$t; tput sgr0; \
		PHPSPY=./phpspy PHP="$(php_cmd)" TEST_SH=$$(dirname $$t)/test.sh ./$$t; ec=$$?; echo; \
		[ $$ec -eq 0 ] && pass=$$((pass+1)); \
		total=$$((total+1)); \
	done; \
	printf "Passed %d out of %d tests\n" $$pass $$total ; \
	[ $$pass -eq $$total ] || exit 1

install: phpspy
	install -D -v -m 755 phpspy $(DESTDIR)$(prefix)/bin/phpspy
	install -D -v -m 755 stackcollapse-phpspy.pl $(DESTDIR)$(prefix)/lib/phpspy/stackcollapse-phpspy.pl
	install -D -v -m 755 vendor/flamegraph.pl $(DESTDIR)$(prefix)/lib/phpspy/vendor/flamegraph.pl
	install -D -v -m 755 phpspy-flamegraph $(DESTDIR)$(prefix)/lib/phpspy/phpspy-flamegraph
	ln -sf $(DESTDIR)$(prefix)/lib/phpspy/phpspy-flamegraph $(DESTDIR)$(prefix)/bin/phpspy-flamegraph

clean:
	rm -f phpspy test.err

.PHONY: all test install clean
