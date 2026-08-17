idf_component_register(
    SRCS
        "main.cpp"
        "board/board_power.cpp"
        "board/board_shared_spi.cpp"
        "core/app_log.cpp"
        "display/display_test_pattern.cpp"
        "display/sticky_display.cpp"
        "ui/canvas.cpp"
        "ui/font.cpp"
    INCLUDE_DIRS
        "."
        "board"
        "core"
        "display"
        "ui"
    REQUIRES
        esp_driver_gpio
        esp_driver_spi
        esp_hw_support
        seeed_epaper
        spi_flash
)

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)
