# phpspy

phpspy is a low-overhead sampling profiler for PHP. It works with non-ZTS PHP
7.0+ with CLI, Apache, and FPM SAPIs on 64-bit Linux 3.2+.

[![Build Status](https://travis-ci.org/adsr/phpspy.svg?branch=master)](https://travis-ci.org/adsr/phpspy)

### Demos

You can profile PHP scripts:

![child](https://i.imgur.com/QE8iJLA.gif)

You can attach to running PHP processes:

![attach cli](https://i.imgur.com/HubBqo0.gif)

![attach httpd](https://i.imgur.com/KVY3KXq.gif)

It has a top-like mode:

![top](https://i.imgur.com/Y0NJJ1C.gif)

It can collect request info, memory usage, and variables:

![advanced](https://i.imgur.com/nlUfCWT.gif)

You can also use it to make flamegraphs like this:

![FlameGraph example](https://i.imgur.com/e6P9Arp.png)

All with no changes to your application and minimal overhead.

### Synopsis

    $ git clone https://github.com/adsr/phpspy.git
    Cloning into 'phpspy'...
    ...
    $ cd phpspy
    $ make
    ...
    $ sudo ./phpspy --limit=1000 --pid=$(pgrep -n httpd) >traces
    ...
    $ ./stackcollapse-phpspy.pl <traces | ./vendor/flamegraph.pl >flame.svg
    $ google-chrome flame.svg # View flame.svg in browser

### Build options

    $ make                   # Use built-in structs
    $ # or
    $ USE_ZEND=1 make ...    # Use Zend structs (requires PHP development headers)

### Usage
    $ ./phpspy -h
    Usage:
      phpspy [options] -p <pid>
      phpspy [options] -P <pgrep-args>
      phpspy [options] [--] <cmd>

    Options:
      -h, --help                         Show this help
      -p, --pid=<pid>                    Trace PHP process at `pid`
      -P, --pgrep=<args>                 Concurrently trace processes that
                                           match pgrep `args` (see also `-T`)
      -T, --threads=<num>                Set number of threads to use with `-P`
                                           (default: 16)
      -s, --sleep-ns=<ns>                Sleep `ns` nanoseconds between traces
                                           (see also `-H`) (default: 10101010)
      -H, --rate-hz=<hz>                 Trace `hz` times per second
                                           (see also `-s`) (default: 99)
      -V, --php-version=<ver>            Set PHP version
                                           (default: auto;
                                           supported: 70 71 72 73 74 80 81 82)
      -l, --limit=<num>                  Limit total number of traces to capture
                                           (approximate limit in pgrep mode)
                                           (default: 0; 0=unlimited)
      -i, --time-limit-ms=<ms>           Stop tracing after `ms` milliseconds
                                           (second granularity in pgrep mode)
                                           (default: 0; 0=unlimited)
      -n, --max-depth=<max>              Set max stack trace depth
                                           (default: -1; -1=unlimited)
      -r, --request-info=<opts>          Set request info parts to capture
                                           (q=query c=cookie u=uri p=path
                                           capital=negation)
                                           (default: QCUP; none)
      -m, --memory-usage                 Capture peak and current memory usage
                                           with each trace (requires target PHP
                                           process to have debug symbols)
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
      -b, --buffer-size=<size>           Set output buffer size to `size`.
                                           Note: In `-P` mode, setting this
                                           above PIPE_BUF (4096) may lead to
                                           interlaced writes across threads
                                           unless `-J m` is specified.
                                           (default: 4096)
      -f, --filter=<regex>               Filter output by POSIX regex
                                           (default: none)
      -F, --filter-negate=<regex>        Same as `-f` except negated
      -d, --verbose-fields=<opts>        Set verbose output fields
                                           (p=pid t=timestamp
                                           capital=negation)
                                           (default: PT; none)
      -c, --continue-on-error            Attempt to continue tracing after
                                           encountering an error
      -#, --comment=<any>                Ignored; intended for self-documenting
                                           commands
      -@, --nothing                      Ignored
      -v, --version                      Print phpspy version and exit

    Experimental options:
      -j, --event-handler=<handler>      Set event handler (fout, callgrind)
                                           (default: fout)
      -J, --event-handler-opts=<opts>    Set event handler options
                                           (fout: m=use mutex to prevent
                                           interlaced writes on stdout in `-P`
                                           mode)
      -S, --pause-process                Pause process while reading stacktrace
                                           (unsafe for production!)
      -e, --peek-var=<varspec>           Peek at the contents of the var located
                                           at `varspec`, which has the format:
                                           <varname>@<path>:<lineno>
                                           <varname>@<path>:<start>-<end>
                                           e.g., xyz@/path/to.php:10-20
      -g, --peek-global=<glospec>        Peek at the contents of a global var
                                           located at `glospec`, which has
                                           the format: <global>.<key>
                                           where <global> is one of:
                                           post|get|cookie|server|files|globals
                                           e.g., server.REQUEST_TIME
      -t, --top                          Show dynamic top-like output

### Example (variable peek)

    $ sudo ./phpspy -e 'i@/var/www/test/lib/test.php:12' -p $(pgrep -n httpd) | grep varpeek
    # varpeek i@/var/www/test/lib/test.php:12 = 42
    # varpeek i@/var/www/test/lib/test.php:12 = 42
    # varpeek i@/var/www/test/lib/test.php:12 = 43
    # varpeek i@/var/www/test/lib/test.php:12 = 44
    ...

### Example (pgrep daemon mode)

    $ sudo ./phpspy -H1 -T4 -P '-x php'
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

    $ ./phpspy -- php -r 'usleep(100000);'
    0 usleep <internal>:-1
    1 <main> <internal>:-1

    0 usleep <internal>:-1
    1 <main> <internal>:-1

    0 usleep <internal>:-1
    1 <main> <internal>:-1

    0 usleep <internal>:-1
    1 <main> <internal>:-1

    0 usleep <internal>:-1
    1 <main> <internal>:-1

    0 usleep <internal>:-1
    1 <main> <internal>:-1

    process_vm_readv: No such process

### Example (cli attach)

    $ php -r 'sleep(10);' &
    [1] 28586
    $ sudo ./phpspy -p 28586
    0 sleep <internal>:-1
    1 <main> <internal>:-1
    ...

### Example (docker)
    $ docker build . -t phpspy
    $ docker run -it --cap-add SYS_PTRACE phpspy:latest ./phpspy/phpspy -V73 -r -- php -r 'sleep(1);'
    0 sleep <internal>:-1
    1 <main> <internal>:-1
    ...

### Known bugs

* phpspy may not work with a chrooted mod_php process whose binary lives inside overlayfs. (See [#109][8].)

### See also

* [rbspy][0] for Ruby, the original inspiration for phpspy
* [py-spy][1] for Python
* [Xdebug profiler][2], instrumented profiler
* [php-profiler][3], similar to phpspy but pure PHP
* [sample_prof][4]
* [php-trace][5]
* [Blackfire][6], commercial
* [Tideways][7], commercial

### TODO

* See `grep -ri todo`
* See https://github.com/adsr/phpspy/issues

[0]: https://github.com/rbspy/rbspy
[1]: https://github.com/benfred/py-spy
[2]: http://www.xdebug.org/docs/profiler
[3]: https://github.com/sj-i/php-profiler
[4]: https://github.com/nikic/sample_prof
[5]: https://github.com/krakjoe/trace
[6]: https://blackfire.io/
[7]: https://tideways.io/
[8]: https://github.com/adsr/phpspy/issues/109
