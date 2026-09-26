# A build that fails the way a compiler fails: a stand-in for its words. The output reader's claim
# (agents/workshop/build-output.md) needs real bytes: GCC's U+2018 and U+2019 quotes, MSVC's CR LF,
# a compile command kilobytes long on one line, a path with a space. They are written to one file
# that `cmake -E cat` prints unchanged (`message()` adds a prefix and `cmake -E echo` cannot say a
# carriage return), and the fixture target fails afterwards. A CMake script, so no shell.
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
