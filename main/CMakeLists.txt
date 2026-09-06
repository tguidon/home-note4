idf_component_register(
    SRCS "app_main.cc" "battery_history.cc" "tennis.cc" "formula_one.cc" "display_worker.cc" "dashboard.cc" "render.cc" "network.cc" "feeds.cc"
         "../components/zectrix_demo_ui/zectrix_canvas.cc"
    INCLUDE_DIRS "." "../components/zectrix_demo_ui/include" "../components/zectrix_demo_ui/font"
    REQUIRES esp_driver_usb_serial_jtag zectrix_board zectrix_epd esp_timer esp_wifi esp_event esp_netif
             esp_http_client esp_http_server mbedtls nvs_flash json lwip
    EMBED_TXTFILES "setup.html")
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
