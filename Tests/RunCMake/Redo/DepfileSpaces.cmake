enable_language(CXX)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/inc dir")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/inc dir/msg header.h"
"#pragma once\n"
"#define MSG \"one\"\n")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/main.cxx"
"#include <iostream>\n"
"#include \"msg header.h\"\n"
"int main()\n"
"{\n"
"  std::cout << MSG << \"\\n\";\n"
"  return 0;\n"
"}\n")

add_executable(hello "${CMAKE_CURRENT_BINARY_DIR}/main.cxx")
target_include_directories(hello PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/inc dir")

