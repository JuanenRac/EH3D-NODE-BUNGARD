# The board profile (Kconfig, sdkconfig.board.<board>): the W5500 driver file is built only for the s3-eth board. The REQUIRES cannot depend on it: ESP-IDF reads them
# before sdkconfig is loaded, so esp_eth and esp_driver_spi are always required (an unused component costs nothing in the image).
set(EH3D_SOURCES "api_shared.cpp" "app_main.cpp" "central_uart.cpp" "clock_sync.cpp" "entropy.cpp" "github_update.cpp" "improv_serial.cpp" "lamps.cpp" "link_server.cpp" "log_buffer.cpp" "machine_api.cpp"
                 "network.cpp" "node_guard.cpp" "node_store.cpp" "psram_mem.cpp" "spindle.cpp" "task_health.cpp" "tls_cert.cpp" "web_server.cpp")
set(EH3D_REQUIRES driver esp_driver_gpio esp_driver_uart esp_driver_ledc esp_event esp_netif esp_timer esp_wifi esp_http_server esp_https_server esp_http_client esp-tls app_update
                  esp_partition esp_app_format mbedtls bootloader_support nvs_flash lwip esp_hw_support esp_system heap log freertos esp_eth esp_driver_spi spi_flash)
if(CONFIG_EH3D_BOARD_S3_ETH)
  list(APPEND EH3D_SOURCES "board_ethernet.cpp")
endif()
idf_component_register(SRCS ${EH3D_SOURCES}
                       INCLUDE_DIRS "." ".."
                       REQUIRES ${EH3D_REQUIRES})
if(CONFIG_EH3D_BOARD_S3_ETH)
  target_compile_definitions(${COMPONENT_LIB} PRIVATE EH3D_BOARD_S3_ETH=1)
endif()

# The web panel (panel/) is compressed at configure time and embedded in the firmware: an update of the firmware updates the panel.
set(EH3D_PANEL_DIR "${PROJECT_DIR}/panel")
set(EH3D_PANEL_OUT "${CMAKE_CURRENT_BINARY_DIR}/panel")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND ${Python3_EXECUTABLE} "${PROJECT_DIR}/tools/pack_panel.py" "${EH3D_PANEL_DIR}" "${EH3D_PANEL_OUT}" RESULT_VARIABLE EH3D_PACK_RESULT)
if(NOT EH3D_PACK_RESULT EQUAL 0)
  message(FATAL_ERROR "tools/pack_panel.py failed")
endif()
set_property(DIRECTORY "${COMPONENT_DIR}" APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${EH3D_PANEL_DIR}/text.js")
foreach(asset index.html app.js style.css)
  set_property(DIRECTORY "${COMPONENT_DIR}" APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${EH3D_PANEL_DIR}/${asset}")
  target_add_binary_data(${COMPONENT_LIB} "${EH3D_PANEL_OUT}/${asset}.gz" BINARY)
endforeach()
