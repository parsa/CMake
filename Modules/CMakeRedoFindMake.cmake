# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

find_program(CMAKE_MAKE_PROGRAM
  NAMES redo
  DOC "Program used to build from redo .do scripts.")
mark_as_advanced(CMAKE_MAKE_PROGRAM)

