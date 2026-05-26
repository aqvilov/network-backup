cmake_minimum_required(VERSION 3.10)
project(NetBackup VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Подключение vcpkg
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
    set(CMAKE_WIN32_EXECUTABLE ON)
endif()

if(MSVC)
    add_compile_options(/utf-8)
endif()

# ========== СНАЧАЛА ОБЪЯВЛЯЕМ ЦЕЛЬ ==========
add_executable(NetBackup WIN32
    src/MainWindow.cpp
    src/GoogleAuth.cpp
    src/GoogleDriveUploader.cpp
    include/Logger.h
    include/Config.h
    include/FileUtils.h
    include/Watcher.h
    include/BackupQueue.h
)

target_include_directories(NetBackup PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ========== ПОТОМ ПОДКЛЮЧАЕМ БИБЛИОТЕКИ ==========
target_link_libraries(NetBackup PRIVATE
    comctl32
    shell32
    ole32
    ws2_32
    crypt32
)

# Поиск OpenSSL (после объявления цели)
find_package(OpenSSL CONFIG QUIET)
if(OpenSSL_FOUND)
    target_link_libraries(NetBackup PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    message(STATUS "OpenSSL found, HTTPS support enabled")
else()
    message(WARNING "OpenSSL not found, HTTPS support disabled")
endif()

# Настройка для MSVC
if(MSVC)
    target_compile_definitions(NetBackup PRIVATE
        _CRT_SECURE_NO_WARNINGS
    )
    # Если OpenSSL не найден, отключаем его в httplib
    if(NOT OpenSSL_FOUND)
        target_compile_definitions(NetBackup PRIVATE
            CPPHTTPLIB_OPENSSL_SUPPORT=0
            CPPHTTPLIB_SSL_ENABLED=0
        )
    endif()
endif()

if(MINGW)
    target_link_options(NetBackup PRIVATE
        -static-libgcc
        -static-libstdc++
        -Wl,--subsystem,windows
    )
    target_compile_definitions(NetBackup PRIVATE
        CPPHTTPLIB_OPENSSL_SUPPORT=0
        CPPHTTPLIB_SSL_ENABLED=0
    )
endif()

set_target_properties(NetBackup PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/Debug
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MSVC)
        target_compile_options(NetBackup PRIVATE /O2)
    elseif(MINGW)
        target_compile_options(NetBackup PRIVATE -O3 -s)
    endif()
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(MSVC)
        target_compile_options(NetBackup PRIVATE /Od)
    elseif(MINGW)
        target_compile_options(NetBackup PRIVATE -O0 -g)
    endif()
endif()