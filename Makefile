phpspy_cflags:=-Wall -Wextra -pedantic -g -Ofast -pthread $(CFLAGS)
phpspy_libs:=$(LDLIBS)
phpspy_includes:=-I.
phpspy_defines:=
prefix?=/usr/local

has_libdw   := $(shell $(LD) -ldw -o/dev/null 		>/dev/null 2>&1 && echo :)
has_pthread := $(shell $(LD) -lpthread -o/dev/null	>/dev/null 2>&1 && echo :)
has_readelf := $(shell command -v readelf     		>/dev/null 2>&1 && echo :)
has_phpconf := $(shell command -v php-config  		>/dev/null 2>&1 && echo :)

$(or $(has_pthread), $(error Need libpthread))

ifdef USE_ZEND
  $(or $(has_phpconf), $(error Need php-config))
  phpspy_includes:=$(phpspy_includes) $$(php-config --includes)
  phpspy_libs:=$(phpspy_libs) -L$$(php-config --prefix)/lib -lphp7
  phpspy_defines:=$(phpspy_defines) -DUSE_ZEND=1
endif

all: phpspy_readelf

phpspy_libdw: check=$(or $(has_libdw), $(error Need libdw))
phpspy_libdw: phpspy_defines:=$(phpspy_defines) -DUSE_LIBDW=1
phpspy_libdw: phpspy_libs:=$(phpspy_libs) -ldw
phpspy_libdw: phpspy

phpspy_readelf: check=$(or $(has_readelf), $(error Need readelf))
phpspy_readelf: phpspy

phpspy: phpspy.c pgrep.c
	@$(check)
	$(CC) $(phpspy_cflags) $(phpspy_includes) $(phpspy_defines) $? -o phpspy $(phpspy_libs)

install: phpspy
	install -D -v -m 755 phpspy $(DESTDIR)$(prefix)/bin/phpspy

clean:
	rm -f phpspy

.PHONY: all phpspy_libdw phpspy_readelf install clean
