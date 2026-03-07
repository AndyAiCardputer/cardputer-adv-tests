idf_component_register(
    SRCS "app_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES m5stack_tab5 usb_host_hid nvs_flash esp_timer esp_lcd
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE BSP_CONFIG_NO_GRAPHIC_LIB=1)
