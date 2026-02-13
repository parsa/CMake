file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/seed.txt" "one\n")

set(outDir "${CMAKE_CURRENT_BINARY_DIR}/gen dir")
set(outFile "${outDir}/file.txt")
set(runlog "${CMAKE_CURRENT_BINARY_DIR}/run.log")

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/gen_dir.cmake" [==[
file(READ "${SEED}" seed)
string(STRIP "${seed}" seed)

file(MAKE_DIRECTORY "${OUTDIR}")
file(WRITE "${OUTDIR}/file.txt" "${seed}\n")

file(APPEND "${RUNLOG}" "run\n")
]==])

add_custom_command(
  OUTPUT "${outDir}"
  BYPRODUCTS "${outFile}"
  COMMAND "${CMAKE_COMMAND}"
          "-DSEED=${CMAKE_CURRENT_BINARY_DIR}/seed.txt"
          "-DOUTDIR=${outDir}"
          "-DRUNLOG=${runlog}"
          -P "${CMAKE_CURRENT_BINARY_DIR}/gen_dir.cmake"
  DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/seed.txt"
          "${CMAKE_CURRENT_BINARY_DIR}/gen_dir.cmake"
  VERBATIM
)

# Two downstream outputs, both depend on the directory output.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/consume.cmake" [==[
file(READ "${INFILE}" content)
string(STRIP "${content}" content)
file(WRITE "${OUTFILE}" "consumed=${content}\n")
]==])

set(cons1 "${CMAKE_CURRENT_BINARY_DIR}/consumed1.txt")
set(cons2 "${CMAKE_CURRENT_BINARY_DIR}/consumed2.txt")

add_custom_command(
  OUTPUT "${cons1}"
  COMMAND "${CMAKE_COMMAND}"
          "-DINFILE=${outFile}"
          "-DOUTFILE=${cons1}"
          -P "${CMAKE_CURRENT_BINARY_DIR}/consume.cmake"
  DEPENDS "${outDir}"
          "${CMAKE_CURRENT_BINARY_DIR}/consume.cmake"
  VERBATIM
)

add_custom_command(
  OUTPUT "${cons2}"
  COMMAND "${CMAKE_COMMAND}"
          "-DINFILE=${outFile}"
          "-DOUTFILE=${cons2}"
          -P "${CMAKE_CURRENT_BINARY_DIR}/consume.cmake"
  DEPENDS "${outDir}"
          "${CMAKE_CURRENT_BINARY_DIR}/consume.cmake"
  VERBATIM
)

add_custom_target(consume1 DEPENDS "${cons1}")
add_custom_target(consume2 DEPENDS "${cons2}")

