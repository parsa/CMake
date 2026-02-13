include(RunCMake)

set(RunCMake_GENERATOR "Redo")
set(RunCMake_GENERATOR_IS_MULTI_CONFIG 0)

if(NOT RunCMake_MAKE_PROGRAM)
  message(FATAL_ERROR "RunCMake_MAKE_PROGRAM must point to 'redo'.")
endif()

get_filename_component(_redo_dir "${RunCMake_MAKE_PROGRAM}" DIRECTORY)
if(_redo_dir)
  if(CMAKE_HOST_WIN32)
    set(ENV{PATH} "${_redo_dir};$ENV{PATH}")
  else()
    set(ENV{PATH} "${_redo_dir}:$ENV{PATH}")
  endif()
endif()
unset(_redo_dir)

function(run_build_and_run testName)
  run_cmake(${testName})

  set(RunCMake_TEST_NO_CLEAN 1)
  set(RunCMake_TEST_BINARY_DIR "${RunCMake_BINARY_DIR}/${testName}-build")
  set(RunCMake_TEST_OUTPUT_MERGE 1)

  run_cmake_command(${testName}-build ${CMAKE_COMMAND} --build .)
  run_cmake_command(${testName}-run ./hello)
endfunction()

run_build_and_run(Exe)
run_build_and_run(LibAndExe)

function(run_HeaderRebuild)
  set(testName HeaderRebuild)
  set(RunCMake_TEST_BINARY_DIR "${RunCMake_BINARY_DIR}/${testName}-build")
  run_cmake(${testName})

  set(RunCMake_TEST_NO_CLEAN 1)
  set(RunCMake_TEST_OUTPUT_MERGE 1)

  run_cmake_command(${testName}-build ${CMAKE_COMMAND} --build .)
  run_cmake_command(${testName}-run1 ./hello)
  file(WRITE "${RunCMake_TEST_BINARY_DIR}/msg.h" "#pragma once\n#define MSG \"two\"\n")
  run_cmake_command(${testName}-rebuild ${CMAKE_COMMAND} --build .)
  run_cmake_command(${testName}-run2 ./hello)
endfunction()
run_HeaderRebuild()

function(run_GeneratedHeader)
  set(testName GeneratedHeader)
  set(RunCMake_TEST_BINARY_DIR "${RunCMake_BINARY_DIR}/${testName}-build")
  run_cmake(${testName})

  set(RunCMake_TEST_NO_CLEAN 1)
  set(RunCMake_TEST_OUTPUT_MERGE 1)

  run_cmake_command(${testName}-build ${CMAKE_COMMAND} --build .)
  run_cmake_command(${testName}-run1 ./hello)
  file(WRITE "${RunCMake_TEST_BINARY_DIR}/generated.h.in"
    "#pragma once\n#define GENERATED_MESSAGE \"gen2\"\n")
  run_cmake_command(${testName}-rebuild ${CMAKE_COMMAND} --build .)
  run_cmake_command(${testName}-run2 ./hello)
endfunction()
run_GeneratedHeader()

function(run_DepfileSpaces)
  set(testName DepfileSpaces)
  set(RunCMake_TEST_BINARY_DIR "${RunCMake_BINARY_DIR}/${testName} build with spaces")
  run_cmake(${testName})

  # Assert the generator uses bulk dep input mode (no per-line loop).
  set(_obj_do "${RunCMake_TEST_BINARY_DIR}/CMakeFiles/hello.dir/main.cxx.o.do")
  if(NOT EXISTS "${_obj_do}")
    message(FATAL_ERROR "Expected object .do file does not exist:\n  ${_obj_do}")
  endif()
  file(READ "${_obj_do}" _obj_do_content)
  string(FIND "${_obj_do_content}" "--from-file" _pos)
  if(_pos LESS 0)
    message(FATAL_ERROR "Expected object .do to contain '--from-file':\n  ${_obj_do}")
  endif()
  unset(_obj_do)
  unset(_obj_do_content)
  unset(_pos)

  set(RunCMake_TEST_NO_CLEAN 1)
  set(RunCMake_TEST_OUTPUT_MERGE 1)

  run_cmake_command(${testName}-build ${CMAKE_COMMAND} --build .)
  run_cmake_command(${testName}-run1 ./hello)
  file(WRITE "${RunCMake_TEST_BINARY_DIR}/inc dir/msg header.h"
    "#pragma once\n#define MSG \"two\"\n")
  run_cmake_command(${testName}-rebuild ${CMAKE_COMMAND} --build .)
  run_cmake_command(${testName}-run2 ./hello)
endfunction()
run_DepfileSpaces()

function(_assert_runlog_count testName expectedCount)
  set(_runlog "${RunCMake_TEST_BINARY_DIR}/run.log")
  if(NOT EXISTS "${_runlog}")
    message(FATAL_ERROR "${testName}: expected run log does not exist:\n  ${_runlog}")
  endif()
  file(STRINGS "${_runlog}" _runs)
  list(LENGTH _runs _runCount)
  if(NOT _runCount EQUAL expectedCount)
    message(FATAL_ERROR "${testName}: expected generator to run ${expectedCount} time(s), got ${_runCount}")
  endif()
  unset(_runlog)
  unset(_runs)
  unset(_runCount)
endfunction()

function(_assert_file_contains testName path needle)
  if(NOT EXISTS "${path}")
    message(FATAL_ERROR "${testName}: expected file does not exist:\n  ${path}")
  endif()
  file(READ "${path}" _content)
  string(FIND "${_content}" "${needle}" _pos)
  if(_pos LESS 0)
    message(FATAL_ERROR "${testName}: expected file to contain '${needle}':\n  ${path}")
  endif()
  unset(_content)
  unset(_pos)
endfunction()

function(run_MultiOutputCustomCommand)
  set(testName MultiOutputCustomCommand)
  set(RunCMake_TEST_BINARY_DIR "${RunCMake_BINARY_DIR}/${testName}-build")
  run_cmake(${testName})

  set(RunCMake_TEST_NO_CLEAN 1)
  set(RunCMake_TEST_OUTPUT_MERGE 1)

  run_cmake_command(${testName}-build1
    ${CMAKE_COMMAND} --build . --parallel 4 --target use1 use2)
  _assert_runlog_count(${testName} 1)

  file(WRITE "${RunCMake_TEST_BINARY_DIR}/seed.txt" "two\n")
  run_cmake_command(${testName}-build2
    ${CMAKE_COMMAND} --build . --parallel 4 --target use1 use2)
  _assert_runlog_count(${testName} 2)
endfunction()
run_MultiOutputCustomCommand()

function(run_Byproducts)
  set(testName Byproducts)
  set(RunCMake_TEST_BINARY_DIR "${RunCMake_BINARY_DIR}/${testName}-build")
  run_cmake(${testName})

  set(RunCMake_TEST_NO_CLEAN 1)
  set(RunCMake_TEST_OUTPUT_MERGE 1)

  run_cmake_command(${testName}-build1
    ${CMAKE_COMMAND} --build . --parallel 4 --target use1 use2)
  _assert_runlog_count(${testName} 1)

  file(WRITE "${RunCMake_TEST_BINARY_DIR}/seed.txt" "two\n")
  run_cmake_command(${testName}-build2
    ${CMAKE_COMMAND} --build . --parallel 4 --target use1 use2)
  _assert_runlog_count(${testName} 2)
endfunction()
run_Byproducts()

function(run_DirectoryOutput)
  set(testName DirectoryOutput)
  set(RunCMake_TEST_BINARY_DIR "${RunCMake_BINARY_DIR}/${testName}-build")
  run_cmake(${testName})

  set(RunCMake_TEST_NO_CLEAN 1)
  set(RunCMake_TEST_OUTPUT_MERGE 1)

  run_cmake_command(${testName}-build1
    ${CMAKE_COMMAND} --build . --parallel 4 --target consume1 consume2)
  _assert_runlog_count(${testName} 1)
  file(READ "${RunCMake_TEST_BINARY_DIR}/consumed1.txt" _c1)
  file(READ "${RunCMake_TEST_BINARY_DIR}/consumed2.txt" _c2)
  if(NOT _c1 MATCHES "^consumed=one")
    message(FATAL_ERROR "${testName}: consumed1.txt content unexpected:\n${_c1}")
  endif()
  if(NOT _c2 MATCHES "^consumed=one")
    message(FATAL_ERROR "${testName}: consumed2.txt content unexpected:\n${_c2}")
  endif()
  unset(_c1)
  unset(_c2)

  file(WRITE "${RunCMake_TEST_BINARY_DIR}/seed.txt" "two\n")
  run_cmake_command(${testName}-build2
    ${CMAKE_COMMAND} --build . --parallel 4 --target consume1 consume2)
  _assert_runlog_count(${testName} 2)
  file(READ "${RunCMake_TEST_BINARY_DIR}/consumed1.txt" _c1b)
  file(READ "${RunCMake_TEST_BINARY_DIR}/consumed2.txt" _c2b)
  if(NOT _c1b MATCHES "^consumed=two")
    message(FATAL_ERROR "${testName}: consumed1.txt content unexpected after rebuild:\n${_c1b}")
  endif()
  if(NOT _c2b MATCHES "^consumed=two")
    message(FATAL_ERROR "${testName}: consumed2.txt content unexpected after rebuild:\n${_c2b}")
  endif()
  unset(_c1b)
  unset(_c2b)
endfunction()
run_DirectoryOutput()

function(run_JobPoolCustomCommand)
  set(testName JobPoolCustomCommand)
  set(RunCMake_TEST_BINARY_DIR "${RunCMake_BINARY_DIR}/${testName}-build")
  run_cmake(${testName})

  set(RunCMake_TEST_NO_CLEAN 1)
  set(RunCMake_TEST_OUTPUT_MERGE 1)
  run_cmake_command(${testName}-build
    ${CMAKE_COMMAND} --build . --parallel 4 --target t1 t2)

  file(READ "${RunCMake_TEST_BINARY_DIR}/out1.txt" _o1)
  file(READ "${RunCMake_TEST_BINARY_DIR}/out2.txt" _o2)
  if(NOT _o1 MATCHES "^one")
    message(FATAL_ERROR "${testName}: out1.txt content unexpected:\n${_o1}")
  endif()
  if(NOT _o2 MATCHES "^two")
    message(FATAL_ERROR "${testName}: out2.txt content unexpected:\n${_o2}")
  endif()
  unset(_o1)
  unset(_o2)
endfunction()
run_JobPoolCustomCommand()

function(run_UsesTerminalCustomCommand)
  set(testName UsesTerminalCustomCommand)
  set(RunCMake_TEST_BINARY_DIR "${RunCMake_BINARY_DIR}/${testName}-build")
  run_cmake(${testName})

  set(RunCMake_TEST_NO_CLEAN 1)
  set(RunCMake_TEST_OUTPUT_MERGE 1)
  run_cmake_command(${testName}-build
    ${CMAKE_COMMAND} --build . --parallel 4 --target t1 t2)

  file(READ "${RunCMake_TEST_BINARY_DIR}/out1.txt" _o1)
  file(READ "${RunCMake_TEST_BINARY_DIR}/out2.txt" _o2)
  if(NOT _o1 MATCHES "^one")
    message(FATAL_ERROR "${testName}: out1.txt content unexpected:\n${_o1}")
  endif()
  if(NOT _o2 MATCHES "^two")
    message(FATAL_ERROR "${testName}: out2.txt content unexpected:\n${_o2}")
  endif()
  unset(_o1)
  unset(_o2)
endfunction()
run_UsesTerminalCustomCommand()

function(run_JobPoolCompileLinkEmit)
  set(testName JobPoolCompileLinkEmit)
  set(RunCMake_TEST_BINARY_DIR "${RunCMake_BINARY_DIR}/${testName}-build")
  run_cmake(${testName})

  # Object compile .do should use source-level JOB_POOL_COMPILE override.
  set(_obj_do "${RunCMake_TEST_BINARY_DIR}/CMakeFiles/hello.dir/main.cxx.o.do")
  _assert_file_contains(${testName} "${_obj_do}" "# redo-pool: two_jobs 2")

  # Link .do should use target JOB_POOL_LINK.
  set(_link_do "${RunCMake_TEST_BINARY_DIR}/hello.do")
  _assert_file_contains(${testName} "${_link_do}" "# redo-pool: link_one 1")
endfunction()
run_JobPoolCompileLinkEmit()

