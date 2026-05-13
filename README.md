# FIRST MVP VERSION OF NETBACKUP

Простой бэкап файлов — следит за папкой и копирует изменения.
Только WinAPI + C++17.

## Структура

```
netbackup-mvp/
├── include/
│   ├── Logger.h       — запись событий в файл
│   ├── Config.h       — сохранение настроек
│   ├── FileUtils.h    — копирование файлов
│   ├── Watcher.h      — слежка за папкой (ReadDirectoryChangesW)
│   └── BackupQueue.h  — очередь задач копирования
├── src/
│   └── MainWindow.cpp — UI + точка входа
└── .vscode/
    └── tasks.json     — задача сборки
```

### Сборка проекта через CMake

```powershell
git clone <ваш-репозиторий>
cd NetBackup
mkdir build && cd build
cmake .. -G "Visual Studio 18 2026" -A Win32
cmake --build . --config Release

**Запуск:**
```powershell
.\bin\Release\NetBackup.exe
```
Добавил новый файл или папку
Добавь файл в CMakeLists.txt:

cmake
add_executable(NetBackup WIN32
    src/mainwindow.cpp
    src/новый_файл.cpp          # добавь сюда
)
Обнови конфигурацию и собери:

powershell
cd build
cmake ..                       # обновить
cmake --build . --config Release
Добавил целую папку с файлами
cmake
# В CMakeLists.txt
file(GLOB NEW_SOURCES "src/новая_папка/*.cpp")
add_executable(NetBackup WIN32
    src/mainwindow.cpp
    ${NEW_SOURCES}
)

---

#### Ручная сборка через Developer Command Prompt
Для сборки в VS должен быть установлен "Developing Desktop Apps C++"

Зайдите в **Developer Command Prompt for VS** и выполните:
```
cd C:\Users\путь\к\вашей\папке
cl /EHsc /std:c++17 /utf-8 /Fe:NetBackup.exe src\MainWindow.cpp /I include /link comctl32.lib shell32.lib ole32.lib user32.lib gdi32.lib /SUBSYSTEM:WINDOWS
```

---

## Что умеет MVP

- **Множественные папки для слежки** - добавляйте сколько угодно папок для отслеживания
- Защита от выбора корня диска (C:\, D:\) и системных папок
- Выбор нескольких папок для бэкапа
- Автоматическое копирование при изменении файлов
- Сохранение структуры подпапок
- Журнал событий с временем
- Статистика: файлов скопировано, ошибки, трафик
- Настройки сохраняются между запусками (%APPDATA%\NetBackup\config.ini)
- Лог файл (%APPDATA%\NetBackup\backup.log)
- Кнопка открыть папку бэкапа

## next feat::

После того как локальный бэкап работает, добавим загрузку в Google Drive:
- OAuth2 авторизация
- Загрузка через Google Drive REST API
