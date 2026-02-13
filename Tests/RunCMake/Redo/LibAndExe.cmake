enable_language(CXX)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/lib.h"
"#pragma once\n"
"const char* greet();\n")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/lib.cxx"
"#include \"lib.h\"\n"
"const char* greet()\n"
"{\n"
"  return \"lib\";\n"
"}\n")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/main.cxx"
"#include <iostream>\n"
"#include \"lib.h\"\n"
"int main()\n"
"{\n"
"  std::cout << greet() << \"\\n\";\n"
"  return 0;\n"
"}\n")

add_library(greet STATIC "${CMAKE_CURRENT_BINARY_DIR}/lib.cxx")
target_include_directories(greet PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")

add_executable(hello "${CMAKE_CURRENT_BINARY_DIR}/main.cxx")
target_link_libraries(hello PRIVATE greet)
target_include_directories(hello PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

