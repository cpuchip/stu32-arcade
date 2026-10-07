get_filename_component(REPO "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
file(GLOB BABAL_SRC "${REPO}/babal/*.c")
set(SCRIPTS_C "${REPO}/build/device/scripts.c")
if(NOT EXISTS "${SCRIPTS_C}")
    message(FATAL_ERROR "no ${SCRIPTS_C}: run scripts/device.sh")
endif()
idf_component_register(
    SRCS bench_main.c "${SCRIPTS_C}" "${REPO}/oracle/out.c" "${REPO}/oracle/game.c" ${BABAL_SRC}
    INCLUDE_DIRS . "${REPO}/oracle" "${REPO}/babal" "${REPO}/build/device")
# As on the host: no fused multiply-adds (the oracle's pins go red with them).
target_compile_options(${COMPONENT_LIB} PRIVATE -ffp-contract=off)
