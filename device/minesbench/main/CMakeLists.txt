# SPDX-License-Identifier: MIT
get_filename_component(REPO "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(FW "${REPO}/build/firmware/firmware")
set(SCRIPTS_C "${REPO}/build/device-mines/appscripts.c")
if(NOT EXISTS "${SCRIPTS_C}")
    message(FATAL_ERROR "no ${SCRIPTS_C}: run scripts/device_mines.sh")
endif()
idf_component_register(
    SRCS minesbench_main.c "${SCRIPTS_C}" "${REPO}/games/mines/mines_app.c" "${REPO}/games/mines/mines_logic.c"
         "${FW}/draw.c" "${FW}/font5x7.c"
    INCLUDE_DIRS . "${REPO}/games/mines" "${FW}" "${REPO}/build/device-mines")
