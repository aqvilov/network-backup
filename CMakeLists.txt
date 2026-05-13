cmake_minimum_required(VERSION 3.10)
project(NetBackup VERSION 1.0.0)

# Устанавливаем стандарт C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Для Windows
if(WIN32)
    # Включаем Unicode
    add_definitions(-DUNICODE -D_UNICODE)
    # Отключаем консольное окно (GUI приложение)
    set(CMAKE_WIN32_EXECUTABLE ON)
endif()

# Устанавливаем кодировку UTF-8 для MSVC
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Исполняемый файл
add_executable(NetBackup WIN32
    src/mainwindow.cpp
    
    # Заголовочные файлы (для отображения в IDE)
    include/Logger.h
    include/Config.h
    include/FileUtils.h
    include/Watcher.h
    include/BackupQueue.h
)

# Директории для поиска заголовков
target_include_directories(NetBackup PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Подключаем необходимые библиотеки Windows
target_link_libraries(NetBackup
    comctl32    # Common Controls (ListView и др.)
    shell32     # Shell функции (SHBrowseForFolder и др.)
    ole32       # COM (CoInitialize/CoUninitialize)
)

# Для MinGW (если используется)
if(MINGW)
    target_link_libraries(NetBackup
        comctl32
        shell32
        ole32
    )
endif()

# Настройки для разных конфигураций сборки
set_target_properties(NetBackup PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/Debug
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release
)

# Копировать файл иконки (если есть)
# if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/resources/app.ico)
#     set_target_properties(NetBackup PROPERTIES
#         LINK_FLAGS "/ENTRY:mainCRTStartup"
#     )
#     target_sources(NetBackup PRIVATE resources/app.rc)
# endif()

# Сборка релиза с оптимизациями
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MSVC)
        target_compile_options(NetBackup PRIVATE /O2 /MT)
    elseif(MINGW)
        target_compile_options(NetBackup PRIVATE -O3 -s)
    endif()
endif()

# Сборка отладки без оптимизаций
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(MSVC)
        target_compile_options(NetBackup PRIVATE /Od /MTd)
    elseif(MINGW)
        target_compile_options(NetBackup PRIVATE -O0 -g)
    endif()
endif()

# Дополнительные опции для MinGW
if(MINGW)
    target_link_options(NetBackup PRIVATE
        -static-libgcc
        -static-libstdc++
        -Wl,--subsystem,windows
    )
endif()