enable_language(CXX)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated.h.in"
"#pragma once\n"
"#define GENERATED_MESSAGE \"gen1\"\n")

add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated.h"
  COMMAND "${CMAKE_COMMAND}" -E copy
          "${CMAKE_CURRENT_BINARY_DIR}/generated.h.in"
          "${CMAKE_CURRENT_BINARY_DIR}/generated.h"
  DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/generated.h.in"
  VERBATIM
)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/main.cxx"
"#include <iostream>\n"
"#include \"generated.h\"\n"
"int main()\n"
"{\n"
"  std::cout << GENERATED_MESSAGE << \"\\n\";\n"
"  return 0;\n"
"}\n")

add_executable(hello "${CMAKE_CURRENT_BINARY_DIR}/main.cxx")
target_sources(hello PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated.h")
target_include_directories(hello PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

