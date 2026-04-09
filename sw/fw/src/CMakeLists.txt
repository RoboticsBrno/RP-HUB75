add_executable(disp
    entry.c
    disp.c
)

pico_set_program_name(disp "RP-HUB75 display driver")
pico_set_program_version(disp "v0 (usb)")

pico_enable_stdio_uart(disp 0)
pico_enable_stdio_usb(disp 1)
target_compile_definitions(disp PRIVATE PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=2500)

pico_generate_pio_header(disp ${CMAKE_CURRENT_LIST_DIR}/hub75.pio)

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
