file(GLOB_RECURSE U8G2_SOURCES "drivers/u8g2/csrc/*.c")

idf_component_register(
    SRCS 
        "tasks/main.cpp"
        "devices/EspNowMaster.cpp"
        "devices/painel.cpp"
        "devices/athlete.cpp"
        "devices/ResultWriter.cpp"         
        "drivers/u8g2_esp32_hal/src/u8g2_esp32_hal.c"
        ${U8G2_SOURCES}
    INCLUDE_DIRS
        "."
        "devices"
        "drivers/u8g2/csrc"
        "drivers/u8g2_esp32_hal/include"
    PRIV_REQUIRES 
        driver
        log
        freertos 
        esp_timer
    REQUIRES 
        nvs_flash
        esp_wifi
        esp_netif
        esp_event
        spiffs          
  
)
