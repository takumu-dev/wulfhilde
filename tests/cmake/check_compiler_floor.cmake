# Runs the configuration's compiler floor check on the compiler named by COMPILER_ID and
# COMPILER_VERSION, as CMake would report them, so a test can try compilers this machine lacks.
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/WulfhildeCompilerFloor.cmake")

wulfhilde_check_compiler_floor("${COMPILER_ID}" "${COMPILER_VERSION}")
