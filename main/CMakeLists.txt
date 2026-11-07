# Main application component.
#
# Layering (low -> high), enforced by review rather than by separate components
# so the whole tree rebuilds together during early bring-up:
#
#   hal/   platform-dependent drivers and services; the only layer that calls
#          ESP-IDF or FreeRTOS directly, apart from esp_log
#   core/  hardware-independent kernel: display manager, event bus, kernel loop
#   gfx/   font binary loader + text layout
#   ui/    self-contained widget library
#   book/  e-book format layer (also compiles on the host for tests)
#   apps/  home / settings / reader / files
#
# Add each subdir to SRC_DIRS as it gains its first source file.

idf_component_register(
    SRC_DIRS
        "."
        "hal"
        "core"
        "gfx"
        "book"
        "ui"
        "ui/widgets"
        "apps"
        "apps/app_home"
        "apps/app_settings"
        "apps/app_wifi"
        "apps/app_time"
        "apps/app_files"
        "apps/app_reader"
        "apps/app_books"
        "apps/app_display"
        "apps/app_about"
        "apps/app_language"
        "apps/app_power"
        "apps/app_sysinfo"
        "apps/lua"
    INCLUDE_DIRS
        "."
        "hal"
        "core"
        "gfx"
        "book"
        "ui"
        "apps"
    REQUIRES
        ssd1677_otp
        driver
        esp_driver_i2c
        esp_driver_sdmmc
        sdmmc
        fatfs
        esp_wifi
        esp_event
        esp_netif
        esp_timer
        esp_rom
        esp_app_format
        nvs_flash
        esp_http_client
        esp-tls
        json
        lua
    EMBED_FILES
        "assets/font_ui.m5f"
        "assets/icons.m5i"
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall -Wextra -Wno-unused-parameter
    -finput-charset=UTF-8 -fexec-charset=UTF-8)  # Korean string literals
