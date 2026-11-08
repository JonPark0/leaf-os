# Host-native unit tests for the hardware-independent layers (book/, and later
# gfx/text_layout). NOT part of the firmware build.
#
#   cmake -S tests -B build/host && cmake --build build/host && ctest --test-dir build/host
#
# Needs any host C++17 compiler (g++, clang, or MSVC). The ESP-IDF cross
# toolchain cannot build these.
cmake_minimum_required(VERSION 3.16)
project(m5papermono_lite_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()

set(BOOK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../main/book)

# Host-clean sources only. zip_reader.cpp / epub_parser.cpp need the ESP32-S3
# ROM inflate, so they are excluded from the host build.
set(BOOK_SRC
    ${BOOK_DIR}/txt_parser.cpp
    ${BOOK_DIR}/xml_lite.cpp
    ${BOOK_DIR}/xhtml_parser.cpp)

add_executable(test_book
    test_main.cpp
    test_txt_parser.cpp
    test_xhtml_parser.cpp
    ${BOOK_SRC})
target_include_directories(test_book PRIVATE ${BOOK_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    target_compile_options(test_book PRIVATE /W4 /utf-8)
else()
    target_compile_options(test_book PRIVATE -Wall -Wextra)
endif()

add_test(NAME book COMMAND test_book)

# appstore/: app.txt parsing, catalog rules, URL host checks.
set(APPSTORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../main/appstore)
set(CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../main/core)

add_executable(test_appstore
    test_main.cpp
    test_appstore.cpp
    ${APPSTORE_DIR}/manifest.cpp
    ${APPSTORE_DIR}/catalog.cpp
    ${APPSTORE_DIR}/url.cpp
    ${CORE_DIR}/kv.cpp)
target_include_directories(test_appstore PRIVATE ${APPSTORE_DIR} ${CORE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    target_compile_options(test_appstore PRIVATE /W4 /utf-8)
else()
    target_compile_options(test_appstore PRIVATE -Wall -Wextra)
endif()

add_test(NAME appstore COMMAND test_appstore)

# ota/: firmware version comparison and feed rules.
set(OTA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../main/ota)

add_executable(test_ota
    test_main.cpp
    test_ota.cpp
    ${OTA_DIR}/firmware_feed.cpp)
target_include_directories(test_ota PRIVATE ${OTA_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    target_compile_options(test_ota PRIVATE /W4 /utf-8)
else()
    target_compile_options(test_ota PRIVATE -Wall -Wextra)
endif()

add_test(NAME ota COMMAND test_ota)
