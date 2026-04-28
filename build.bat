@echo off
REM Скрипт для быстрой компиляции NetBackup
REM Требуется установленная Visual Studio 2019 или новее

echo ====================================
echo    NetBackup - Quick Build Script
echo ====================================
echo.

REM Инициализация окружения Visual Studio 2019
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

REM Компиляция проекта
echo.
echo Компиляция NetBackup.exe...
echo.

cl /EHsc /std:c++17 /utf-8 /Fe:NetBackup.exe src\MainWindow.cpp /I include /link comctl32.lib shell32.lib ole32.lib user32.lib gdi32.lib /SUBSYSTEM:WINDOWS

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ====================================
    echo   Сборка успешно завершена!
    echo   Создан файл: NetBackup.exe
    echo ====================================
) else (
    echo.
    echo ====================================
    echo   Ошибка при сборке!
    echo ====================================
)

echo.
pause
