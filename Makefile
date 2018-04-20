phpspy_cflags:=$(CFLAGS) -Wall -Wextra -g
phpspy_libs:=$(LDLIBS)
phpspy_includes:=-I.
phpspy_defines:=

has_libdw   := $(shell $(LD) -ldw -o/dev/null >/dev/null 2>&1 && echo 1)
has_readelf := $(shell command -v readelf     >/dev/null 2>&1 && echo 1)
has_phpconf := $(shell command -v php-config  >/dev/null 2>&1 && echo 1)

ifdef USE_ZEND
  $(or $(has_phpconf), $(error Need php-config))
  phpspy_includes:=$(phpspy_includes) $$(php-config --includes)
  phpspy_defines:=$(phpspy_defines) -DUSE_ZEND=1
endif

all: phpspy_libdw

phpspy_libdw: check:=$(or $(has_libdw), $(error Need libdw))
phpspy_libdw: phpspy_defines:=$(phpspy_defines) -DUSE_LIBDW=1
phpspy_libdw: phpspy_libs:=$(phpspy_libs) -ldw
phpspy_libdw: phpspy

phpspy_readelf: check:=$(or $(has_readelf), $(error Need readelf))
phpspy_readelf: phpspy

phpspy: phpspy.c
	$(CC) $(phpspy_cflags) $(phpspy_includes) $(phpspy_defines) phpspy.c -o phpspy $(phpspy_libs)

clean:
	rm -f phpspy

.PHONY: all phpspy_libdw phpspy_readelf clean
