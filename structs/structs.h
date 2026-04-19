#ifdef USE_ZEND
#  include <main/php_config.h>
#  undef ZEND_DEBUG
#  define ZEND_DEBUG 0
#  include <main/SAPI.h>
#  undef snprintf
#  undef vsnprintf
#  undef HASH_ADD
#else
#  if defined(__x86_64__)
#    include <structs/x86_64/php_structs_70.h>
#    include <structs/x86_64/php_structs_71.h>
#    include <structs/x86_64/php_structs_72.h>
#    include <structs/x86_64/php_structs_73.h>
#    include <structs/x86_64/php_structs_74.h>
#    include <structs/x86_64/php_structs_80.h>
#    include <structs/x86_64/php_structs_81.h>
#    include <structs/x86_64/php_structs_82.h>
#    include <structs/x86_64/php_structs_83.h>
#    include <structs/x86_64/php_structs_84.h>
#    include <structs/x86_64/php_structs_85.h>
#    include <structs/x86_64/php_structs_86.h>
#  elif defined(__aarch64__)
#    include <structs/aarch64/php_structs_70.h>
#    include <structs/aarch64/php_structs_71.h>
#    include <structs/aarch64/php_structs_72.h>
#    include <structs/aarch64/php_structs_73.h>
#    include <structs/aarch64/php_structs_74.h>
#    include <structs/aarch64/php_structs_80.h>
#    include <structs/aarch64/php_structs_81.h>
#    include <structs/aarch64/php_structs_82.h>
#    include <structs/aarch64/php_structs_83.h>
#    include <structs/aarch64/php_structs_84.h>
#    include <structs/aarch64/php_structs_85.h>
#    include <structs/aarch64/php_structs_86.h>
#  else
#    #error "Architecture unsupported"
#  endif
#endif
