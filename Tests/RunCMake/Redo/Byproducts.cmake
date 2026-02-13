file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/seed.txt" "one\n")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/gen_byproducts.cmake" [==[
file(READ "${SEED}" seed)
string(STRIP "${seed}" seed)

file(WRITE "${OUT1}"
"#pragma once\n"
"#define GEN1 \"${seed}\"\n")

file(WRITE "${OUT2}"
"#pragma once\n"
"#define GEN2 \"${seed}\"\n")

file(APPEND "${RUNLOG}" "run\n")
]==])

set(out1 "${CMAKE_CURRENT_BINARY_DIR}/gen1.h")
set(out2 "${CMAKE_CURRENT_BINARY_DIR}/gen2.h")
set(runlog "${CMAKE_CURRENT_BINARY_DIR}/run.log")

add_custom_command(
  OUTPUT "${out1}"
  BYPRODUCTS "${out2}"
  COMMAND "${CMAKE_COMMAND}"
          "-DSEED=${CMAKE_CURRENT_BINARY_DIR}/seed.txt"
          "-DOUT1=${out1}"
          "-DOUT2=${out2}"
          "-DRUNLOG=${runlog}"
          -P "${CMAKE_CURRENT_BINARY_DIR}/gen_byproducts.cmake"
  DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/seed.txt"
          "${CMAKE_CURRENT_BINARY_DIR}/gen_byproducts.cmake"
  VERBATIM
)

add_custom_target(use1 DEPENDS "${out1}")
add_custom_target(use2 DEPENDS "${out2}")

