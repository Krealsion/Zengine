# A build that fails the way a compiler fails: a stand-in for the WORDS of one.
#
# The output reader (agents/workshop/build-output.md) claims a maker can read a compiler's own
# diagnostic inside Workshop -- its path, its line and column, its quotes -- and a claim about
# those bytes needs those bytes. GCC in a UTF-8 locale quotes identifiers with U+2018 and U+2019;
# MSVC ends a line with CR LF; a build tool echoes a compile command several kilobytes long on one
# line; and a maker's checkout path may hold a space. This script writes exactly such output, as
# one file, and `cmake -E cat` puts its bytes on the build's stdout unchanged. The fixture target
# that runs it fails afterwards (tests/buildfixture/CMakeLists.txt), so the whole ends as a failed
# build does.
#
# IT IS A CMAKE SCRIPT, for `slow_build.cmake`'s reason: no shell, on either platform. The bytes
# are written to a file first because `message()` adds its own prefix and `cmake -E echo` cannot
# say a carriage return.
#
#   OUT   where to write the output before printing it

if(NOT DEFINED OUT)
    message(FATAL_ERROR "diagnostic_build.cmake: -DOUT=<file> is required")
endif()

string(ASCII 13 zen_cr)
# ONE LINE OF 5,000 AND SOME BYTES: longer than a `BuildOutput` (2,048) and longer than a kept line
# (4,096), so it is split in transit and cut, counted, where it is kept.
string(REPEAT "-I/home/maker/zen checkout/include/a/path/a/command/echo/carries " 80 zen_echo)

file(WRITE "${OUT}"
"[1/2] Building CXX object attention-pane/CMakeFiles/zengine-attention-pane.dir/pane.cpp.o\n"
"FAILED: attention-pane/CMakeFiles/zengine-attention-pane.dir/pane.cpp.o \n"
"/usr/bin/c++ ${zen_echo}-c /home/maker/zen checkout/attention-pane/pane.cpp\n"
"/home/maker/zen checkout/attention-pane/pane.cpp: In member function ‘void {anonymous}::AttentionPaneWeave::say_view(Push&&)’:\n"
"/home/maker/zen checkout/attention-pane/pane.cpp:416:23: error: ‘oops’ was not declared in this scope\n"
"  416 |         push(\"ATTENTION\" + oops);\n"
"      |                            ^~~~\n"
"pane.cpp(416): error C2065: 'oops': undeclared identifier${zen_cr}\n"
"ninja: build stopped: subcommand failed.\n")

execute_process(COMMAND "${CMAKE_COMMAND}" -E cat "${OUT}")
