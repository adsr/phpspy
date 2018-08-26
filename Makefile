phpspy_cflags:=-Wall -Wextra -g -Ofast $(CFLAGS)
phpspy_libs:=$(LDLIBS)
phpspy_includes:=-I.
phpspy_defines:=
DESTDIR?=/usr/local/bin/

has_libdw   := $(shell $(LD) -ldw -o/dev/null >/dev/null 2>&1 && echo :)
has_readelf := $(shell command -v readelf     >/dev/null 2>&1 && echo :)
has_phpconf := $(shell command -v php-config  >/dev/null 2>&1 && echo :)

ifdef USE_ZEND
  $(or $(has_phpconf), $(error Need php-config))
  phpspy_includes:=$(phpspy_includes) $$(php-config --includes)
  phpspy_defines:=$(phpspy_defines) -DUSE_ZEND=1
endif

all: phpspy_readelf

phpspy_libdw: check=$(or $(has_libdw), $(error Need libdw))
phpspy_libdw: phpspy_defines:=$(phpspy_defines) -DUSE_LIBDW=1
phpspy_libdw: phpspy_libs:=$(phpspy_libs) -ldw
phpspy_libdw: phpspy

phpspy_readelf: check=$(or $(has_readelf), $(error Need readelf))
phpspy_readelf: phpspy

phpspy: phpspy.c
	@$(check)
	$(CC) $(phpspy_cflags) $(phpspy_includes) $(phpspy_defines) phpspy.c -o phpspy $(phpspy_libs)

install: phpspy
	install -v -m 755 phpspy $(DESTDIR)

clean:
	rm -f phpspy

.PHONY: all phpspy_libdw phpspy_readelf clean
