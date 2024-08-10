cmake_minimum_required (VERSION 3.8)

project ("phpspy")

set(CMAKE_C_STANDARD 11)

ADD_DEFINITIONS(-DWINDOWS)

add_executable (phpspy basic.c phpspy.c addr_objdump.c event_fout.c event_callgrind.c vendor/getopt.c)
