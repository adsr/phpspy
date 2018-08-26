# phpspy

Low-overhead sampling profiler for PHP 7 inspired by [rbspy][0].

For now, works with Linux x86_64 non-ZTS PHP 7 with CLI and Apache SAPIs.

You can use it to make flamegraphs like this:

![FlameGraph example](https://i.imgur.com/7DKdnmh.gif)

### Synopsis

    $ git clone https://github.com/adsr/phpspy.git
    Cloning into 'phpspy'...
    ...
    $ cd phpspy
    $ make
    cc  -Wall -Wextra -g -Ofast -I.  phpspy.c -o phpspy
    $ sudo ./phpspy -l 1000 -p $(pgrep -n httpd) | ./stackcollapse-phpspy.pl | ./flamegraph.pl > flame.svg
    ...
    $ google-chrome flame.svg # view flame.svg in browser

### Build

    $ make
    cc  -Wall -Wextra -g -Ofast -I.  phpspy.c -o phpspy
    $ ./phpspy -h
    
    Usage: phpspy [options] [--] <php_command>

    -h         Show help
    -p <pid>   Trace PHP process at pid
    -s <ns>    Sleep this many nanoseconds between traces (default: 10000000, 10ms)
    -n <max>   Set max stack trace depth to `max` (default: -1, unlimited)
    -x <hex>   Address of executor_globals in hex (default: 0, find dynamically)
    -a <hex>   Address of sapi_globals in hex (default: 0, find dynamically)
    -r         Capture request info as well
    -l <num>   Limit number of stack traces to capture (default: 0, unlimited)
    -o <path>  Write phpspy output to path instead of stdout (default: -)
    -O <path>  Write child stdout to path instead of stdout (default: phpspy.%d.out)
    -E <path>  Write child stderr to path instead of stderr (default: phpspy.%d.err)

### Build options

    $ make phpspy_readelf  # Use readelf (default) binary (requires elfutils)
    $ # or
    $ make phpspy_libdw    # Use libdw (requires libdw-dev or elfutils-devel)
    $ # also
    $ USE_ZEND=1 make ...  # Use Zend structs instead of built-in structs (requires php-dev or php-devel)

### Example (httpd)

    $ sudo ./phpspy -p $(pgrep -n httpd)
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
    # - - -
    ...

### Example (cli child)

    $ sudo ./phpspy -r -- php -r 'usleep(100000);'
    0 usleep <internal>:-1
    1 <main> <internal>:-1
    # 1535179291.009094 - Standard input code
    
    0 usleep <internal>:-1
    1 <main> <internal>:-1
    # 1535179291.009094 - Standard input code
    
    0 usleep <internal>:-1
    1 <main> <internal>:-1
    # 1535179291.009094 - Standard input code
    
    0 usleep <internal>:-1
    1 <main> <internal>:-1
    # 1535179291.009094 - Standard input code
    
    0 usleep <internal>:-1
    1 <main> <internal>:-1
    # 1535179291.009094 - Standard input code
    
    0 usleep <internal>:-1
    1 <main> <internal>:-1
    # 1535179291.009094 - Standard input code
    
    process_vm_readv: No such process

### Example (cli attach)

    $ php -r 'sleep(10);' &
    [1] 28586
    $ sudo ./phpspy -p 28586
    0 sleep <internal>:-1
    1 <main> <internal>:-1
    # - - -
    ...

### TODO

* See https://github.com/adsr/phpspy/issues

[0]: https://github.com/rbspy/rbspy
