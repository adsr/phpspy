phpspy_cflags:=-Wall -Wextra -pedantic -g -Ofast -pthread $(CFLAGS)
phpspy_libs:=$(LDLIBS)
phpspy_ldflags:=$(LDFLAGS)
phpspy_includes:=-I.
phpspy_defines:=
prefix?=/usr/local

sinclude config.mk

has_libdw   := $(shell $(LD) $(phpspy_ldflags) -ldw      -o/dev/null >/dev/null 2>&1 && echo :)
has_pthread := $(shell $(LD) $(phpspy_ldflags) -lpthread -o/dev/null >/dev/null 2>&1 && echo :)
has_termbox := $(shell $(LD) $(phpspy_ldflags) -ltermbox -o/dev/null >/dev/null 2>&1 && echo :)
has_readelf := $(shell command -v readelf                            >/dev/null 2>&1 && echo :)
has_phpconf := $(shell command -v php-config                         >/dev/null 2>&1 && echo :)

$(or $(has_pthread), $(error Need libpthread))

ifdef USE_ZEND
  $(or $(has_phpconf), $(error Need php-config))
  phpspy_includes:=$(phpspy_includes) $$(php-config --includes)
  phpspy_defines:=$(phpspy_defines) -DUSE_ZEND=1
endif

ifeq ($(has_termbox), :)
  USE_TERMBOX=1
endif
ifdef USE_TERMBOX
  $(or $(has_termbox), $(error Need libtermbox))
  phpspy_libs:=$(phpspy_libs) -ltermbox
  phpspy_defines:=$(phpspy_defines) -DUSE_TERMBOX=1
endif

all: phpspy_readelf

phpspy_libdw: check=$(or $(has_libdw), $(error Need libdw))
phpspy_libdw: phpspy_defines:=$(phpspy_defines) -DUSE_LIBDW=1
phpspy_libdw: phpspy_libs:=$(phpspy_libs) -ldw
phpspy_libdw: phpspy

phpspy_readelf: check=$(or $(has_readelf), $(error Need readelf))
phpspy_readelf: phpspy_defines:=$(phpspy_defines) -DUSE_READELF=1
phpspy_readelf: phpspy

phpspy: phpspy.c pgrep.c top.c addr_libdw.c addr_readelf.c
	@$(check)
	$(CC) $(phpspy_cflags) $(phpspy_includes) $(phpspy_defines) $? -o phpspy $(phpspy_ldflags) $(phpspy_libs)

install: phpspy
	install -D -v -m 755 phpspy $(DESTDIR)$(prefix)/bin/phpspy

clean:
	rm -f phpspy

.PHONY: all phpspy_libdw phpspy_readelf install clean
