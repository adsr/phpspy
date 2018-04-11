# phpspy

Experimental sampling profiler for PHP 7 inspired by [rbspy][0].

For now, Linux x86_64 only, non-ZTS PHP 7 only.

### Synopsis

    [user@host:~/phpspy] $ make
    cc -Wall -Wextra -g -I. phpspy.c -o phpspy
    [user@host:~/phpspy] $ ./phpspy -h
    Usage: phpspy -h(help) -p<pid> -s<sleep_us> -n<max_stack_depth> -x<executor_globals_addr>
    [user@host:~/phpspy] $ php -r 'sleep(120);' &
    [1] 28586
    [user@host:~/phpspy] $ sudo ./phpspy -p 28586
    0 sleep <internal>:-1
    1 <main> <internal>:-1
    ...
    [user@host:~/phpspy] $ sudo ./phpspy -p $(pgrep httpd)
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

* FlameGraph trace format

[0]: https://github.com/rbspy/rbspy
