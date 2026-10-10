# SPDX-License-Identifier: MIT
get_filename_component(REPO "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(FW "${REPO}/build/firmware/firmware")
set(SCRIPTS_C "${REPO}/build/device-aim/appscripts.c")
if(NOT EXISTS "${SCRIPTS_C}")
    message(FATAL_ERROR "no ${SCRIPTS_C}: run scripts/device_aim.sh")
endif()
idf_component_register(
    SRCS aimbench_main.c "${SCRIPTS_C}" "${REPO}/games/aim/aim_app.c" "${REPO}/games/aim/aim_logic.c" "${FW}/draw.c"
         "${FW}/font5x7.c"
    INCLUDE_DIRS . "${REPO}/games/aim" "${FW}" "${REPO}/build/device-aim")
