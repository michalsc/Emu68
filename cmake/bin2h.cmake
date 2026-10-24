# Copyright © 2020 Michal Schulz <michal.schulz@gmx.de>
# https://github.com/michalsc
#
# This Source Code Form is subject to the terms of the
# Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
# with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

function(bin_to_header target)

    find_program(XXD xxd REQUIRED)

    cmake_path(GET target FILENAME TARGET_NAME)
    cmake_path(GET target PARENT_PATH TARGET_PATH)
    message(${TARGET_PATH})

    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${target}.h
        COMMAND ${XXD} -i ${TARGET_NAME} > ${CMAKE_CURRENT_BINARY_DIR}/include/${TARGET_NAME}.h
        DEPENDS ${target}
        COMMENT "Generating ${TARGET_NAME}.h from binary."
        WORKING_DIRECTORY ${TARGET_PATH}
        VERBATIM
    )

    add_custom_target(
        ${TARGET_NAME}_header ALL
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${target}.h
    )

endfunction(bin_to_header)
