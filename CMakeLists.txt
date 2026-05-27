cmake_minimum_required(VERSION 3.16)
project(NetBackup VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# Vcpkg toolchain support
# ============================================================================
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND EXISTS "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

if(NOT DEFINED VCPKG_TARGET_TRIPLET)
    set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "vcpkg target triplet")
endif()

# ============================================================================
# Find OpenSSL
# ============================================================================
find_package(OpenSSL QUIET)

if(OPENSSL_FOUND)
    message(STATUS "Found OpenSSL: ${OPENSSL_VERSION}")
    message(STATUS "   Include: ${OPENSSL_INCLUDE_DIR}")
    message(STATUS "   Libs:    ${OPENSSL_LIBRARIES}")
else()
    message(WARNING "OpenSSL not found. Google Drive features will be disabled.")
    message(WARNING "Install OpenSSL via: vcpkg install openssl:x64-windows")
endif()

# ============================================================================
# Try to find nlohmann_json (optional)
# ============================================================================
find_package(nlohmann_json QUIET)

if(nlohmann_json_FOUND)
    message(STATUS "Found nlohmann_json: ${nlohmann_json_VERSION}")
else()
    message(STATUS "nlohmann_json not found, will use simple JSON implementation if needed")
endif()

# ============================================================================
# Try to find cpp-httplib (optional)
# ============================================================================
find_package(httplib QUIET)

if(httplib_FOUND)
    message(STATUS "Found cpp-httplib")
else()
    message(STATUS "cpp-httplib not found, will use simple HTTP implementation if needed")
endif()

# ============================================================================
# Windows / compiler settings
# ============================================================================
if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
    set(CMAKE_WIN32_EXECUTABLE ON)
endif()

if(MSVC)
    add_compile_options(/utf-8)
    add_compile_options(/wd4146)
endif()

# ============================================================================
# Google Drive source files (общие для основного таргета и тестов)
# ============================================================================
set(GOOGLE_SOURCES "")

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/GoogleAuth.cpp)
    list(APPEND GOOGLE_SOURCES src/GoogleAuth.cpp)
    message(STATUS "GoogleAuth.cpp added")
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/GoogleDriveUploader.cpp)
    list(APPEND GOOGLE_SOURCES src/GoogleDriveUploader.cpp)
    message(STATUS "GoogleDriveUploader.cpp added")
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/GoogleDriveUploadQueue.cpp)
    list(APPEND GOOGLE_SOURCES src/GoogleDriveUploadQueue.cpp)
    message(STATUS "GoogleDriveUploadQueue.cpp added")
endif()

# ============================================================================
# Вспомогательная функция: общие настройки для тестовых таргетов
# ============================================================================
function(configure_test_target TARGET_NAME)
    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_compile_definitions(${TARGET_NAME} PRIVATE
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        _CONSOLE
    )
    target_link_libraries(${TARGET_NAME} PRIVATE shell32 ws2_32 crypt32)
    if(OPENSSL_FOUND)
        target_include_directories(${TARGET_NAME} PRIVATE ${OPENSSL_INCLUDE_DIR})
        target_link_libraries(${TARGET_NAME} PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    endif()
    if(nlohmann_json_FOUND)
        target_link_libraries(${TARGET_NAME} PRIVATE nlohmann_json::nlohmann_json)
    endif()
    if(MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE /utf-8 /wd4146)
        set_target_properties(${TARGET_NAME} PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
    endif()
endfunction()

# ============================================================================
# Target: NetBackup
# ============================================================================
set(SOURCES src/MainWindow.cpp ${GOOGLE_SOURCES})

add_executable(NetBackup WIN32 ${SOURCES})

target_include_directories(NetBackup PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(NetBackup PRIVATE
    comctl32
    shell32
    ole32
    ws2_32
    crypt32
)

if(OPENSSL_FOUND)
    target_include_directories(NetBackup PRIVATE ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(NetBackup PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()

if(nlohmann_json_FOUND)
    target_link_libraries(NetBackup PRIVATE nlohmann_json::nlohmann_json)
endif()

if(MINGW)
    target_link_libraries(NetBackup PRIVATE gdi32)
endif()

set_target_properties(NetBackup PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_BINARY_DIR}/bin/Debug
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release
)

# ============================================================================
# Tests
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    message(STATUS "Found tests directory")
    enable_testing()

    # --------------------------------------------------------------------------
    # test_fileutils
    # --------------------------------------------------------------------------
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_fileutils.cpp)
        add_executable(test_fileutils
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_fileutils.cpp
        )
        configure_test_target(test_fileutils)
        add_test(NAME FileUtilsTest COMMAND test_fileutils)
        set_tests_properties(FileUtilsTest PROPERTIES TIMEOUT 30)
        message(STATUS "test_fileutils configured")
    endif()

    # --------------------------------------------------------------------------
    # test_watcher
    # --------------------------------------------------------------------------
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_watcher.cpp)
        add_executable(test_watcher
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_watcher.cpp
        )
        configure_test_target(test_watcher)
        add_test(NAME WatcherTest COMMAND test_watcher)
        set_tests_properties(WatcherTest PROPERTIES TIMEOUT 60)
        message(STATUS "test_watcher configured")
    endif()

    # --------------------------------------------------------------------------
    # test_config
    # --------------------------------------------------------------------------
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/config_test.cpp)
        add_executable(test_config
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/config_test.cpp
        )
        configure_test_target(test_config)
        add_test(NAME ConfigTest COMMAND test_config)
        set_tests_properties(ConfigTest PROPERTIES TIMEOUT 30)
        message(STATUS "test_config configured")
    endif()

    # --------------------------------------------------------------------------
    # test_backup_queue
    # BackupQueue.h включает GoogleDriveUploader.h — нужны .cpp реализации
    # --------------------------------------------------------------------------
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/backup_queue_test.cpp)
        add_executable(test_backup_queue
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/backup_queue_test.cpp
            ${GOOGLE_SOURCES}   # GoogleAuth, GoogleDriveUploader, GoogleDriveUploadQueue
        )
        configure_test_target(test_backup_queue)
        target_link_libraries(test_backup_queue PRIVATE ole32)
        add_test(NAME BackupQueueTest COMMAND test_backup_queue)
        set_tests_properties(BackupQueueTest PROPERTIES TIMEOUT 30)
        message(STATUS "test_backup_queue configured")
    endif()


    # test_logger
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_logger.cpp)
        add_executable(test_logger
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_logger.cpp
        )
        configure_test_target(test_logger)
        add_test(NAME LoggerTest COMMAND test_logger)
        set_tests_properties(LoggerTest PROPERTIES TIMEOUT 30)
        message(STATUS "test_logger configured")
    endif()

    message(STATUS "Tests configured")
endif()

# ============================================================================
# Build type settings
# ============================================================================
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MSVC)
        target_compile_options(NetBackup PRIVATE /O2 /MT)
    elseif(MINGW)
        target_compile_options(NetBackup PRIVATE -O3 -s)
    endif()
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(MSVC)
        target_compile_options(NetBackup PRIVATE /Od /MTd)
    elseif(MINGW)
        target_compile_options(NetBackup PRIVATE -O0 -g)
    endif()
endif()

if(MINGW)
    target_link_options(NetBackup PRIVATE
        -static-libgcc
        -static-libstdc++
        -Wl,--subsystem,windows
    )
endif()

# ============================================================================
# Summary
# ============================================================================
message(STATUS "")
message(STATUS "========================================")
message(STATUS "NetBackup Configuration Summary")
message(STATUS "========================================")
message(STATUS "Build type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
if(OPENSSL_FOUND)
    message(STATUS "OpenSSL:      ${OPENSSL_VERSION}")
else()
    message(STATUS "OpenSSL:      NOT FOUND (Google Drive disabled)")
endif()
message(STATUS "Triplet:      ${VCPKG_TARGET_TRIPLET}")
message(STATUS "========================================")
message(STATUS "")