function(download_raspi_firmware)
    set(PI_FILES
        LICENCE.broadcom
        bootcode.bin
        fixup.dat
        fixup4.dat
        start.elf
        start4.elf
        bcm2711-rpi-4-b.dtb
        bcm2711-rpi-400.dtb
        bcm2711-rpi-cm4.dtb
        bcm2710-rpi-cm3.dtb
        bcm2710-rpi-3-b.dtb
        bcm2710-rpi-3-b-plus.dtb
        bcm2710-rpi-zero-2.dtb
        bcm2710-rpi-zero-2-w.dtb
    )
    message("-- Downloading RasPi firmware files")
    foreach(F IN LISTS PI_FILES)
        if(NOT EXISTS ${CMAKE_BINARY_DIR}/firmware/${F})
            message("--   ${F}")
            file(DOWNLOAD https://github.com/raspberrypi/firmware/raw/master/boot/${F} ${CMAKE_BINARY_DIR}/firmware/${F})
        endif()
        install(FILES ${CMAKE_BINARY_DIR}/firmware/${F} DESTINATION .)
    endforeach()
endfunction(download_raspi_firmware)
