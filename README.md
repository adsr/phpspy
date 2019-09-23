# phpspy

phpspy is a low-overhead sampling profiler for PHP 7. For now, it works with
Linux 3.2+ x86_64 non-ZTS PHP 7.0+ with CLI, Apache, and FPM SAPIs.

[![Build Status](https://travis-ci.org/adsr/phpspy.svg?branch=master)](https://travis-ci.org/adsr/phpspy)

It has a top-like mode:

![top mode](https://i.imgur.com/E8QTUfE.gif)

You can peek at variables at runtime:

![varpeek](https://i.imgur.com/HlpPd0x.gif)

You can also use it make flamegraphs like this:

![FlameGraph example](https://i.imgur.com/7DKdnmh.gif)

All with no changes to your application and minimal overhead.

### Synopsis

    $ git clone https://github.com/adsr/phpspy.git
    Cloning into 'phpspy'...
    ...
    $ cd phpspy
    $ make
    ...
    $ sudo ./phpspy -l 1000 -p $(pgrep -n httpd) | ./stackcollapse-phpspy.pl | ./vendor/flamegraph.pl > flame.svg
    ...
    $ google-chrome flame.svg # View flame.svg in browser

### Build options

    $ make                   # Build dependencies locally and statically link
    $ # or
    $ make phpspy_dynamic    # Dynamically link dependencies
    $ # or
    $ USE_ZEND=1 make ...    # Use Zend structs instead of built-in structs (requires php-dev or php-devel)

### Usage
    $ ./phpspy -h
    Usage:
      phpspy [options] -p <pid>
      phpspy [options] -P <pgrep-args>
      phpspy [options] -- <cmd>

    Options:
      -h, --help                         Show this help
      -p, --pid=<pid>                    Trace PHP process at `pid`
      -P, --pgrep=<args>                 Concurrently trace processes that match
                                           pgrep `args` (see also `-T`)
      -T, --threads=<num>                Set number of threads to use with `-P`
                                           (default: 16)
      -s, --sleep-ns=<ns>                Sleep `ns` nanoseconds between traces
                                           (see also `-H`) (default: 10101010)
      -H, --rate-hz=<hz>                 Trace `hz` times per second
                                           (see also `-s`) (default: 99)
      -V, --php-version=<ver>            Set PHP version
                                           (default: auto;
                                           supported: 70 71 72 73 74)
      -l, --limit=<num>                  Limit total number of traces to capture
                                           (default: 0; 0=unlimited)
      -i, --time-limit-ms=<ms>           Stop tracing after `ms` milliseconds
                                           (default: 0; 0=unlimited)
      -n, --max-depth=<max>              Set max stack trace depth
                                           (default: -1; -1=unlimited)
      -r, --request-info=<opts>          Set request info parts to capture
                                           (q=query c=cookie u=uri p=path
                                           capital=negation)
                                           (default: QCUP; none)
      -m, --memory-usage                 Capture peak and current memory usage
                                           with each trace (requires PHP debug
                                           symbols)
      -o, --output=<path>                Write phpspy output to `path`
                                           (default: -; -=stdout)
      -O, --child-stdout=<path>          Write child stdout to `path`
                                           (default: phpspy.%d.out)
      -E, --child-stderr=<path>          Write child stderr to `path`
                                           (default: phpspy.%d.err)
      -x, --addr-executor-globals=<hex>  Set address of executor_globals in hex
                                           (default: 0; 0=find dynamically)
      -a, --addr-sapi-globals=<hex>      Set address of sapi_globals in hex
                                           (default: 0; 0=find dynamically)
      -1, --single-line                  Output in single-line mode
      -f, --filter=<regex>               Filter output by POSIX regex
                                           (default: none)
      -F, --filter-negate=<regex>        Same as `-f` except negated
      -#, --comment=<any>                Ignored; intended for self-documenting
                                           commands
      -@, --nothing                      Ignored
      -v, --version                      Print phpspy version and exit

    Experimental options:
      -S, --pause-process                Pause process while reading stacktrace
                                           (unsafe for production!)
      -e, --peek-var=<varspec>           Peek at the contents of the var located
                                           at `varspec`, which has the format:
                                           <varname>@<path>:<lineno>
                                           <varname>@<path>:<start>-<end>
                                           e.g., xyz@/path/to.php:10-20
      -g, --peek-global=<glospec>        Peek at the contents of a superglobal var
                                           located at `glospec`, which has the
                                           format: <global>.<key>
                                           where <global> is one of:
                                           post, get, cookies, server, env, files,
                                           e.g., server.request_id
      -t, --top                          Show dynamic top-like output


### Example (variable peek)

    $ sudo ./phpspy -V73 -e 'i@/var/www/test/lib/test.php:12' -p $(pgrep -n httpd) | grep varpeek
    # varpeek i@/var/www/test/lib/test.php:12 = 42
    # varpeek i@/var/www/test/lib/test.php:12 = 42
    # varpeek i@/var/www/test/lib/test.php:12 = 43
    # varpeek i@/var/www/test/lib/test.php:12 = 44
    ...

### Example (pgrep daemon mode)

    $ sudo ./phpspy -V73 -H1 -T4 -P '-x php'
    0 proc_open <internal>:-1
    1 system_with_timeout /home/adam/php-src/run-tests.php:1137
    2 run_test /home/adam/php-src/run-tests.php:1937
    3 run_all_tests /home/adam/php-src/run-tests.php:1215
    4 <main> /home/adam/php-src/run-tests.php:986
    # - - - - -
    ...
    ^C
    main_pgrep finished gracefully

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
    # - - - - -
    ...

### Example (cli child)

    $ sudo ./phpspy -r -- php -r 'usleep(100000);'
    0 usleep <internal>:-1
    1 <main> <internal>:-1
    # 1535179291.009094 - - Standard input code -

    0 usleep <internal>:-1
    1 <main> <internal>:-1
    # 1535179291.009094 - - Standard input code -

    0 usleep <internal>:-1
    1 <main> <internal>:-1
    # 1535179291.009094 - - Standard input code -

    0 usleep <internal>:-1
    1 <main> <internal>:-1
    # 1535179291.009094 - - Standard input code -

    0 usleep <internal>:-1
    1 <main> <internal>:-1
    # 1535179291.009094 - - Standard input code -

    0 usleep <internal>:-1
    1 <main> <internal>:-1
    # 1535179291.009094 - - Standard input code -

    process_vm_readv: No such process

### Example (cli attach)

    $ php -r 'sleep(10);' &
    [1] 28586
    $ sudo ./phpspy -p 28586
    0 sleep <internal>:-1
    1 <main> <internal>:-1
    # - - - - -
    ...

### Example (docker)
    $ docker build . -t phpspy
    $ docker run -it --cap-add SYS_PTRACE phpspy:latest ./phpspy/phpspy -V73 -r -- php -r 'sleep(1);'
    0 sleep <internal>:-1
    1 <main> <internal>:-1
    ...

### Credits

* phpspy is inspired by [rbspy][0].

### See also

* [rbspy][0]
* [py-spy][1]

### TODO

* See `grep -ri todo`
* See https://github.com/adsr/phpspy/issues

[0]: https://github.com/rbspy/rbspy
[1]: https://github.com/benfred/py-spy
