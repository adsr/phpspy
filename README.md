# phpspy

Experimental sampling profiler for PHP 7 inspired by [rbspy][0].

For now, Linux x86_64 only, non-ZTS PHP 7 only.

### Synopsis

    [user@host:~/phpspy] $ make
    cc -Wall -Wextra -g -I. phpspy.c -o phpspy
    [user@host:~/phpspy] $ php -r 'sleep(120);' &
    [1] 28586
    [user@host:~/phpspy] $ sudo ./phpspy -p 28586
    0 sleep <internal>:-1
    1 <main> <internal>:-1
    ...
    ^C
    [user@host:~/phpspy] $ sudo ./phpspy -p $(pgrep httpd)
    0 get <internal>:-1
    1 get /foo/bar/lib/Cache.php:26
    2 get /foo/bar/lib/Cache.php:251
    3 isGuest /foo/bar/lib/User.php:54
    4 findRecord /foo/bar/lib/User.php:22
    5 __construct /foo/bar/lib/Rule/User.php:40
    6 init /foo/bar/lib/Rule/RequestContext.php:34
    7 __construct /foo/bar/lib/Rule/Context.php:44
    10 evaluateRules /foo/bar/lib/Rule/Engine.php:116
    12 <main> /foo/bar/htdocs/v3/index.php:5
    ...
    ^C

### TODO

* Display methods as Class::method
* FlameGraph trace format

[0] https://github.com/rbspy/rbspy
