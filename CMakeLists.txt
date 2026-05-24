cmake_minimum_required(VERSION 3.10)
project(NetBackup VERSION 1.0.0)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)

    add_definitions(-DUNICODE -D_UNICODE)
    set(CMAKE_WIN32_EXECUTABLE ON)
endif()


if(MSVC)
    add_compile_options(/utf-8)
endif()


add_executable(NetBackup WIN32
    src/MainWindow.cpp
    

    include/Logger.h
    include/Config.h
    include/FileUtils.h
    include/Watcher.h
    include/BackupQueue.h
)


target_include_directories(NetBackup PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)


target_link_libraries(NetBackup
    comctl32   
    shell32    
    ole32       # COM (CoInitialize/CoUninitialize)
)


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

add_executable(MainWindow_test
    tests/MainWindow_test.cpp
)

target_include_directories(MainWindow_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(MainWindow_test
    comctl32
    shell32
    ole32
)

enable_testing()
add_test(NAME MainWindow_test COMMAND MainWindow_test)

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
