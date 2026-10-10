# SPDX-License-Identifier: MIT
get_filename_component(REPO "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(FW "${REPO}/build/firmware/firmware")
set(SCRIPTS_C "${REPO}/build/device-snake/appscripts.c")
if(NOT EXISTS "${SCRIPTS_C}")
    message(FATAL_ERROR "no ${SCRIPTS_C}: run scripts/device_snake.sh")
endif()
idf_component_register(
    SRCS snakebench_main.c "${SCRIPTS_C}" "${REPO}/games/snake/snake_app.c" "${REPO}/games/snake/snake_logic.c"
         "${FW}/draw.c" "${FW}/font5x7.c"
    INCLUDE_DIRS . "${REPO}/games/snake" "${FW}" "${REPO}/build/device-snake")
