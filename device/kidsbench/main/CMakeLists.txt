# SPDX-License-Identifier: MIT
get_filename_component(REPO "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(FW "${REPO}/build/firmware/firmware")
set(SCRIPTS_C "${REPO}/build/device-kids/appscripts.c")
if(NOT EXISTS "${SCRIPTS_C}")
    message(FATAL_ERROR "no ${SCRIPTS_C}: run scripts/device_kids.sh")
endif()
idf_component_register(
    SRCS kidsbench_main.c "${SCRIPTS_C}"
         "${REPO}/games/3720/rocks_app.c" "${REPO}/games/3720/rocks_logic.c"
         "${REPO}/games/moles/moles_app.c" "${REPO}/games/moles/moles_logic.c"
         "${REPO}/games/catch/catch_app.c" "${REPO}/games/catch/catch_logic.c"
         "${FW}/draw.c" "${FW}/font5x7.c"
    INCLUDE_DIRS . "${REPO}/games/3720" "${REPO}/games/moles" "${REPO}/games/catch" "${REPO}/games/kids" "${FW}"
                 "${REPO}/build/device-kids")
