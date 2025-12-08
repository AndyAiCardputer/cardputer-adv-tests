idf_component_register(
    SRCS "app_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES m5stack_tab5 usb_host_hid nvs_flash
)

# Display functions are available even with BSP_CONFIG_NO_GRAPHIC_LIB=1
# They use only ESP-IDF API, not LVGL
target_compile_definitions(${COMPONENT_LIB} PRIVATE BSP_CONFIG_NO_GRAPHIC_LIB=1)

