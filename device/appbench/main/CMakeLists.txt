# SPDX-License-Identifier: MIT
get_filename_component(REPO "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
file(GLOB BABAL_SRC "${REPO}/games/babal/core/*.c")
set(FW "${REPO}/build/firmware/firmware")
set(APPS_C "${REPO}/build/device-app/appscripts.c")
if(NOT EXISTS "${APPS_C}")
    message(FATAL_ERROR "no ${APPS_C}: run scripts/device_app.sh")
endif()
idf_component_register(
    SRCS appbench_main.c "${APPS_C}" "${REPO}/games/babal/app/babal_app.c" "${REPO}/games/babal/app/render.c" "${FW}/font5x7.c" ${BABAL_SRC}
    INCLUDE_DIRS . "${REPO}/games/babal/app" "${REPO}/games/babal/core" "${FW}" "${REPO}/build/device-app")
target_compile_options(${COMPONENT_LIB} PRIVATE -ffp-contract=off)
