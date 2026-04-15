add_executable(disp
    entry.c
    disp.c
    recv.c
)

pico_set_program_name(disp "Saturn Ring (RP-HUB75) driver")

if (SR_INPUT_USB)
    pico_set_program_version(disp "v0 (usb)")
    target_compile_definitions(disp PRIVATE SR_ENABLE_INPUT_USB)
else()
    pico_set_program_version(disp "v0 (qspi)")
    target_compile_definitions(disp PRIVATE SR_ENABLE_INPUT_QSPI)
endif()

pico_enable_stdio_uart(disp 0)
pico_enable_stdio_usb(disp 1)

pico_generate_pio_header(disp ${CMAKE_CURRENT_LIST_DIR}/hub75.pio)
pico_generate_pio_header(disp ${CMAKE_CURRENT_LIST_DIR}/qrx.pio)

target_link_libraries(disp
    hardware_dma
    hardware_pio
    hardware_irq
    hardware_gpio

    pico_stdlib
    pico_multicore
)

target_include_directories(disp PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

pico_add_extra_outputs(disp)
