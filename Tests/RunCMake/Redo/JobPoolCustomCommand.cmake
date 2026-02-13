set_property(GLOBAL PROPERTY JOB_POOLS pool1=1)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/pool_task.cmake" [==[
if(NOT DEFINED LOCK_FILE OR LOCK_FILE STREQUAL "")
  message(FATAL_ERROR "LOCK_FILE not set")
endif()
if(NOT DEFINED OUT OR OUT STREQUAL "")
  message(FATAL_ERROR "OUT not set")
endif()
if(NOT DEFINED NAME OR NAME STREQUAL "")
  message(FATAL_ERROR "NAME not set")
endif()

# Fail if two tasks overlap.
file(LOCK "${LOCK_FILE}" TIMEOUT 0)
execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 1)
file(WRITE "${OUT}" "${NAME}\n")
]==])

set(lock "${CMAKE_CURRENT_BINARY_DIR}/pool.lock")
set(out1 "${CMAKE_CURRENT_BINARY_DIR}/out1.txt")
set(out2 "${CMAKE_CURRENT_BINARY_DIR}/out2.txt")

add_custom_command(
  OUTPUT "${out1}"
  COMMAND "${CMAKE_COMMAND}"
          "-DLOCK_FILE=${lock}"
          "-DOUT=${out1}"
          "-DNAME=one"
          -P "${CMAKE_CURRENT_BINARY_DIR}/pool_task.cmake"
  DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pool_task.cmake"
  JOB_POOL pool1
  VERBATIM
)

add_custom_command(
  OUTPUT "${out2}"
  COMMAND "${CMAKE_COMMAND}"
          "-DLOCK_FILE=${lock}"
          "-DOUT=${out2}"
          "-DNAME=two"
          -P "${CMAKE_CURRENT_BINARY_DIR}/pool_task.cmake"
  DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pool_task.cmake"
  JOB_POOL pool1
  VERBATIM
)

add_custom_target(t1 DEPENDS "${out1}")
add_custom_target(t2 DEPENDS "${out2}")

