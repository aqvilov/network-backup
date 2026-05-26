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
# OpenSSL
# ============================================================================
find_package(OpenSSL REQUIRED COMPONENTS SSL Crypto)

message(STATUS "Found OpenSSL: ${OPENSSL_VERSION}")
message(STATUS "   Include: ${OPENSSL_INCLUDE_DIR}")
message(STATUS "   Libs:    ${OPENSSL_LIBRARIES}")

# ============================================================================
# External dependencies
# ============================================================================
include(FetchContent)

FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

FetchContent_Declare(httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.15.3
)
FetchContent_MakeAvailable(httplib)

# ============================================================================
# Windows / compiler settings
# ============================================================================
if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
    set(CMAKE_WIN32_EXECUTABLE ON)
endif()

if(MSVC)
    add_compile_options(/utf-8)
    add_compile_options(/wd4146)  # Отключаем warning C4146
endif()

# ============================================================================
# Target: NetBackup
# ============================================================================
add_executable(NetBackup WIN32
    src/MainWindow.cpp
    src/GoogleAuth.cpp
    src/GoogleDriveUploader.cpp
    src/GoogleDriveUploadQueue.cpp
)

target_include_directories(NetBackup PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${json_SOURCE_DIR}/include
    ${httplib_SOURCE_DIR}
    ${OPENSSL_INCLUDE_DIR}
)

target_link_libraries(NetBackup PRIVATE
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto
    comctl32
    shell32
    ole32
    ws2_32
    crypt32
)

if(MINGW)
    target_link_libraries(NetBackup PRIVATE gdi32)
endif()

set_target_properties(NetBackup PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/Debug
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release
)

# ============================================================================
# Tests
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    message(STATUS "Found tests directory")

    set(CONFIG_MOCK_CONTENT
        "#pragma once\n"
        "#include <string>\n"
        "#include <vector>\n"
        "#include <algorithm>\n"
        "#include <cwctype>\n"
        "class Config {\n"
        "public:\n"
        "    static inline bool IsVersionedExtension(const std::wstring& ext) {\n"
        "        std::wstring lower = ext;\n"
        "        for (auto& c : lower) c = towlower(c);\n"
        "        return (lower == L\".txt\" || lower == L\".cpp\" || lower == L\".h\" || lower == L\".c\");\n"
        "    }\n"
        "    static inline bool IsExtensionIgnored(const std::wstring& filename) {\n"
        "        std::wstring lower = filename;\n"
        "        for (auto& c : lower) c = towlower(c);\n"
        "        return (lower == L\".tmp\" || lower == L\".bak\" || \n"
        "                lower == L\"desktop.ini\" || lower == L\"thumbs.db\");\n"
        "    }\n"
        "    static inline int GetMaxVersions() { return 5; }\n"
        "    static inline std::wstring Get(const std::wstring&, const std::wstring& def = L\"\") { return def; }\n"
        "    static inline bool Has(const std::wstring&) { return false; }\n"
        "    static inline std::vector<std::wstring> GetWatchPaths() { return {}; }\n"
        "    static inline void Set(const std::wstring&, const std::wstring&) {}\n"
        "    static inline bool Save() { return true; }\n"
        "};\n"
    )

    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test_helpers)
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/test_helpers/Config.h ${CONFIG_MOCK_CONTENT})

    # test_fileutils
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_fileutils.cpp)
        add_executable(test_fileutils ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_fileutils.cpp)
        target_include_directories(test_fileutils PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_BINARY_DIR}/test_helpers
        )
        target_compile_definitions(test_fileutils PRIVATE
            _CRT_SECURE_NO_WARNINGS
            WIN32_LEAN_AND_MEAN
            _CONSOLE  
        )
        target_link_libraries(test_fileutils PRIVATE shell32)
        if(MSVC)
            target_compile_options(test_fileutils PRIVATE /utf-8 /wd4146)
            set_target_properties(test_fileutils PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
        endif()
        message(STATUS "test_fileutils configured")
    endif()

    # test_logger
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_logger.cpp)
        add_executable(test_logger ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_logger.cpp)
        target_include_directories(test_logger PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        )
        target_compile_definitions(test_logger PRIVATE
            _CRT_SECURE_NO_WARNINGS
            WIN32_LEAN_AND_MEAN
            _CONSOLE
        )
        if(MSVC)
            target_compile_options(test_logger PRIVATE /utf-8 /wd4146)
            set_target_properties(test_logger PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
        endif()
        message(STATUS "test_logger configured")
    endif()

    # test_watcher
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_watcher.cpp)
        add_executable(test_watcher ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_watcher.cpp)
        target_include_directories(test_watcher PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        )
        target_compile_definitions(test_watcher PRIVATE
            _CRT_SECURE_NO_WARNINGS
            WIN32_LEAN_AND_MEAN
            _CONSOLE
        )
        target_link_libraries(test_watcher PRIVATE shell32 ws2_32)
        if(MSVC)
            target_compile_options(test_watcher PRIVATE /utf-8 /wd4146)
            set_target_properties(test_watcher PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
        endif()
        message(STATUS "test_watcher configured")
    endif()

    # CTest
    enable_testing()

    if(TARGET test_fileutils)
        add_test(NAME FileUtilsTest COMMAND test_fileutils)
        set_tests_properties(FileUtilsTest PROPERTIES TIMEOUT 30)
    endif()
    
    if(TARGET test_logger)
        add_test(NAME LoggerTest COMMAND test_logger)
        set_tests_properties(LoggerTest PROPERTIES TIMEOUT 30)
    endif()
    
    if(TARGET test_watcher)
        add_test(NAME WatcherTest COMMAND test_watcher)
        set_tests_properties(WatcherTest PROPERTIES TIMEOUT 60)
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
message(STATUS "OpenSSL:      ${OPENSSL_VERSION}")
message(STATUS "Triplet:      ${VCPKG_TARGET_TRIPLET}")
message(STATUS "========================================")
message(STATUS "")