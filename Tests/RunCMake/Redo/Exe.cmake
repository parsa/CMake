enable_language(CXX)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/main.cxx"
"#include <iostream>\n"
"int main()\n"
"{\n"
"  std::cout << \"exe\\n\";\n"
"  return 0;\n"
"}\n")

add_executable(hello "${CMAKE_CURRENT_BINARY_DIR}/main.cxx")

