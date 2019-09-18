CMAKE_MINIMUM_REQUIRED (VERSION 2.6)

SET(CMAKE_BUILD_TYPE Debug CACHE STRING "Default Build Type")

PROJECT (phpspy)

ADD_CUSTOM_TARGET(submodules
                  COMMAND git submodule update --init --remote --recursive)

ADD_CUSTOM_TARGET(libtermbox 
                  COMMAND ${PROJECT_SOURCE_DIR}/vendor/termbox/waf configure --prefix=${PROJECT_BUILD_DIR}
                  COMMAND ${PROJECT_SOURCE_DIR}/vendor/termbox/waf --targets=termbox_static
                  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/vendor/termbox
                  DEPENDS submodules)

INCLUDE_DIRECTORIES(.)
INCLUDE_DIRECTORIES(vendor)
INCLUDE_DIRECTORIES(vendor/termbox/src)

ADD_EXECUTABLE (phpspy phpspy.c pgrep.c top.c addr_objdump.c event_fout.c)
ADD_DEPENDENCIES (phpspy libtermbox)

TARGET_LINK_LIBRARIES (phpspy pthread ${PROJECT_SOURCE_DIR}/vendor/termbox/build/src/libtermbox.a)

INSTALL (TARGETS phpspy
        RUNTIME DESTINATION bin)

INSTALL (FILES ${PROJECT_SOURCE_DIR}/phpspy.h
        DESTINATION include/phpspy)

INSTALL (FILES ${PROJECT_SOURCE_DIR}/php_structs_70.h
        DESTINATION include/phpspy)
INSTALL (FILES ${PROJECT_SOURCE_DIR}/php_structs_71.h
        DESTINATION include/phpspy)
INSTALL (FILES ${PROJECT_SOURCE_DIR}/php_structs_72.h
        DESTINATION include/phpspy)
INSTALL (FILES ${PROJECT_SOURCE_DIR}/php_structs_73.h
        DESTINATION include/phpspy)
INSTALL (FILES ${PROJECT_SOURCE_DIR}/php_structs_74.h
        DESTINATION include/phpspy)
INSTALL (FILES ${PROJECT_SOURCE_DIR}/vendor/uthash.h
        DESTINATION include/phpspy)
        
SET_TARGET_PROPERTIES(phpspy PROPERTIES 
    COMPILE_FLAGS "-std=c99 -Wall -Werror"
)

