# phpspy

Experimental sampling profiler for PHP 7 inspired by [rbspy][0].

For now, Linux x86_64 only, non-ZTS PHP 7 only.

### Build options

    $ make phpspy_libdw    # Get addr via libdw (default) (requires libdw-dev or elfutils-devel)
    $ make phpspy_readelf  # Get addr via readelf binary (requires elfutils)
    $ USE_ZEND=1 make ...  # Use Zend structs instead of built-in structs (requires php-dev or php-devel)

### Synopsis

    $ make
    cc  -Wall -Wextra -g -I.  -DUSE_LIBDW=1 phpspy.c -o phpspy  -ldw
    $ ./phpspy -h
    Usage: phpspy -h(help) -p<pid> -s<sleep_us> -n<max_stack_depth> -x<executor_globals_addr>
    $ php -r 'sleep(120);' &
    [1] 28586
    $ sudo ./phpspy -p 28586
    0 sleep <internal>:-1
    1 <main> <internal>:-1
    ...
    $ sudo ./phpspy -p $(pgrep httpd)
    0 Memcached::get <internal>:-1
    1 Cache_MemcachedToggleable::get /foo/bar/lib/Cache/MemcachedToggleable.php:26
    2 Cache_Memcached::get /foo/bar/lib/Cache/Memcached.php:251
    3 IpDb_CacheBase::getFromCache /foo/bar/lib/IpDb/CacheBase.php:165
    4 IpDb_CacheBase::get /foo/bar/lib/IpDb/CacheBase.php:107
    5 IpDb_CacheBase::contains /foo/bar/lib/IpDb/CacheBase.php:70
    6 IpDb_Botnet::has /foo/bar/lib/IpDb/Botnet.php:32
    7 Security_Rule_IpAddr::__construct /foo/bar/lib/Security/Rule/IpAddr.php:53
    8 Security_Rule_HttpRequestContext::initVariables /foo/bar/lib/Security/Rule/HttpRequestContext.php:22
    9 Security_Rule_Context::__construct /foo/bar/lib/Security/Rule/Context.php:44
    10 Security_Rule_Engine::getContextByName /foo/bar/lib/Security/Rule/Engine.php:225
    11 Security_Rule_Engine::getContextByLocation /foo/bar/lib/Security/Rule/Engine.php:210
    12 Security_Rule_Engine::evaluateActionRules /foo/bar/lib/Security/Rule/Engine.php:116
    13 <main> /foo/bar/lib/bootstrap/api.php:49
    14 <main> /foo/bar/htdocs/v3/public.php:5
    ...

### TODO

* Add option to specify sample rate in hertz instead of micro-sec
* Add option to grab `$_SERVER[...]` variables along with stack frame
* Add option to exit when pid no longer exists
* Add aggregate mode
* Add `make install` rule

[0]: https://github.com/rbspy/rbspy
