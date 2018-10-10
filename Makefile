phpspy_cflags:=-std=c90 -Wall -Wextra -pedantic -g -Ofast -pthread $(CFLAGS)
phpspy_libs:=$(LDLIBS) -ltermbox
phpspy_ldflags:=$(LDFLAGS)
phpspy_includes:=-I.
phpspy_defines:=-DUSE_TERMBOX=1
phpspy_sources:=phpspy.c pgrep.c top.c addr_libdw.c addr_readelf.c
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

all: phpspy_libdw

libdw:
ifeq ($(has_libdw),)
	cd vendor/elfutils && autoreconf -if && ./configure --enable-maintainer-mode && $(MAKE) && $(MAKE) install
else
	@echo "has libdw"
endif
phpspy_libs:=$(phpspy_libs) -ldw
phpspy_defines:=$(phpspy_defines) -DUSE_LIBDW=1


libtermbox:
ifeq ($(has_termbox),)
	cd vendor/termbox && ./waf configure && ./waf install --targets=termbox_shared
else
	@echo "has libtermbox"
endif


update_deps:
	git submodule update

clean_deps:
	cd vendor/elfutils && make uninstall
	cd vendor/termbox  && ./waf uninstall

phpspy_libdw: update_deps libdw libtermbox
phpspy_libdw: phpspy

phpspy_readelf: check=$(or $(has_readelf), $(error Need readelf))
phpspy_readelf: phpspy_defines:=$(phpspy_defines) -DUSE_READELF=1
phpspy_readelf: update_deps libtermbox
phpspy_readelf: phpspy

phpspy: $(wildcard *.c *.h)
	@$(check)
	$(CC) $(phpspy_cflags) $(phpspy_includes) $(phpspy_defines) $(phpspy_sources) -o phpspy $(phpspy_ldflags) $(phpspy_libs)

install: phpspy
	install -D -v -m 755 phpspy $(DESTDIR)$(prefix)/bin/phpspy

clean:
	rm -f phpspy

.PHONY: all phpspy_libdw phpspy_readelf clean_deps update_deps libdw libtermbox install clean
