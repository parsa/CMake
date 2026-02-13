enable_language(CXX)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/msg.h"
"#pragma once\n"
"#define MSG \"one\"\n")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/main.cxx"
"#include <iostream>\n"
"#include \"msg.h\"\n"
"int main()\n"
"{\n"
"  std::cout << MSG << \"\\n\";\n"
"  return 0;\n"
"}\n")

add_executable(hello "${CMAKE_CURRENT_BINARY_DIR}/main.cxx")
target_include_directories(hello PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

