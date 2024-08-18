cmake_minimum_required (VERSION 3.11)

project ("phpspy")

set(CMAKE_C_STANDARD 11)

include_directories(${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/vendor)
add_definitions(-DPHPSPY_WIN32)

# pcre2
include(FetchContent)
FetchContent_Declare(
  pcre
  GIT_REPOSITORY https://github.com/PCRE2Project/pcre2.git
  GIT_TAG 6ae58beca071f13ccfed31d03b3f479ab520639b	#pcre2-10.44
  GIT_SUBMODULES ""
)
FetchContent_MakeAvailable(pcre)

add_executable(phpspy basic.c phpspy.c pgrep.c addr_objdump.c event_fout.c event_callgrind.c vendor/getopt.c)

target_link_libraries(phpspy pcre2-posix)