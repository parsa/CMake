# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.10)
project(RedoSmoke LANGUAGES CXX)

add_library(greet STATIC lib.cpp)

add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated.h"
  COMMAND "${CMAKE_COMMAND}" -E copy
          "${CMAKE_CURRENT_SOURCE_DIR}/generated.h.in"
          "${CMAKE_CURRENT_BINARY_DIR}/generated.h"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/generated.h.in"
  VERBATIM
)

add_executable(hello main.cpp)
target_link_libraries(hello PRIVATE greet)
target_sources(hello PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated.h")
target_include_directories(hello PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

