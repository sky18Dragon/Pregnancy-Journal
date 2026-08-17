idf_component_register(
    SRCS
        "main.cpp"
        "board/board_power.cpp"
        "core/app_log.cpp"
    INCLUDE_DIRS
        "."
        "board"
        "core"
    REQUIRES
        esp_driver_gpio
        esp_hw_support
        spi_flash
)

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)

