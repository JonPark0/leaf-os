# LEAF OS — Lightweight/Lua EPD Application Framework
# A PDA-style e-reader OS for the M5Stack PaperMono-Lite (SKU C153-Lite).
# Top-level ESP-IDF project file.
cmake_minimum_required(VERSION 3.16)

# ---------------------------------------------------------------------------
# ESP-IDF version guard.
#
# arduino-esp32 3.3.x is only compatible with ESP-IDF v5.5.x, and the M5Stack
# UserDemo patch set we mirror is a diff against that tree. A mismatched IDF
# either fails to link or misbehaves subtly at runtime, so refuse early.
# ---------------------------------------------------------------------------
if(DEFINED ENV{IDF_VERSION})
    set(_idf_ver "$ENV{IDF_VERSION}")
else()
    set(_idf_ver "${IDF_VERSION}")
endif()

if(_idf_ver AND NOT _idf_ver MATCHES "^5\\.5\\.")
    message(FATAL_ERROR
        "This project requires ESP-IDF 5.5.x (found ${_idf_ver}). "
        "arduino-esp32 3.3.x is pinned to that line. "
        "Set -DM5PM_ALLOW_ANY_IDF=1 only if you know what you are doing.")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(leaf_os)
