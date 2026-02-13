set_property(GLOBAL PROPERTY JOB_POOLS two_jobs=2 ten_jobs=10 link_one=1)

enable_language(CXX)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/main.cxx" [==[
#include <iostream>
int main() { std::cout << "hi\n"; return 0; }
]==])

add_executable(hello "${CMAKE_CURRENT_BINARY_DIR}/main.cxx")

# Target-wide compile pool.
set_property(TARGET hello PROPERTY JOB_POOL_COMPILE ten_jobs)
# Source-specific compile pool override (must take precedence).
set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/main.cxx"
  PROPERTIES JOB_POOL_COMPILE two_jobs)

# Link pool.
set_property(TARGET hello PROPERTY JOB_POOL_LINK link_one)

