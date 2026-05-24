cmake_minimum_required(VERSION 3.10)
project(NetBackup VERSION 1.0.0)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Подключение vcpkg
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

# FetchContent для библиотек
include(FetchContent)

# nlohmann/json
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

# cpp-httplib
FetchContent_Declare(httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.15.3
)
FetchContent_MakeAvailable(httplib)

if(WIN32)

    add_definitions(-DUNICODE -D_UNICODE)
    set(CMAKE_WIN32_EXECUTABLE ON)
endif()


if(MSVC)
    add_compile_options(/utf-8)
endif()


add_executable(NetBackup WIN32
    src/MainWindow.cpp
    
    src/GoogleAuth.cpp
    src/GoogleDriverUpLoader.cpp

    include/Logger.h
    include/Config.h
    include/FileUtils.h
    include/Watcher.h
    include/BackupQueue.h
)


target_include_directories(NetBackup PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${nlohmann_json_SOURCE_DIR}/include   # для <nlohmann/json.hpp>
    ${httplib_SOURCE_DIR}                 # для <httplib.h>
)


target_link_libraries(NetBackup
    comctl32   
    shell32    
    ole32       # COM (CoInitialize/CoUninitialize)
    ssl crypto #Для https
)

# Если используете MSVC, может понадобиться явно указать библиотеки для OpenSSL
if(MSVC)
    target_link_libraries(NetBackup
        libssl.lib
        libcrypto.lib
    )
endif()


if(MINGW)
    target_link_libraries(NetBackup
        comctl32
        shell32
        ole32
    )
endif()

set_target_properties(NetBackup PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/Debug
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release
)


# if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/resources/app.ico)
#     set_target_properties(NetBackup PROPERTIES
#         LINK_FLAGS "/ENTRY:mainCRTStartup"
#     )
#     target_sources(NetBackup PRIVATE resources/app.rc)
# endif()


if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MSVC)
        target_compile_options(NetBackup PRIVATE /O2 /MT)
    elseif(MINGW)
        target_compile_options(NetBackup PRIVATE -O3 -s)
    endif()
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
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
