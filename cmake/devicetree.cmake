find_program(DTC dtc REQUIRED)

function(build_devicetree FILES)
    set(OVERLAY_BIN_LIST "")

    foreach(F IN LISTS ${FILES})
        cmake_path(GET F FILENAME F_NAME)
        cmake_path(REPLACE_EXTENSION F_NAME dtbo OUTPUT_VARIABLE F_BIN)
        list(APPEND OVERLAY_BIN_LIST ${CMAKE_CURRENT_BINARY_DIR}/overlays/${F_BIN})
        add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/overlays/${F_BIN}
            COMMAND ${DTC} -o ${CMAKE_CURRENT_BINARY_DIR}/overlays/${F_BIN} ${CMAKE_CURRENT_SOURCE_DIR}/${F}
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${F}
            VERBATIM
        )
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/overlays/${F_BIN} DESTINATION overlays)
    endforeach()

    add_custom_target(__make_sure_devicetree_overlays_are_built ALL DEPENDS ${OVERLAY_BIN_LIST})
endfunction()
