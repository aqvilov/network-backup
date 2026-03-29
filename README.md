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

## Как собрать

### Требования
- Windows 10/11
- Visual Studio Build Tools 2019 или 2022
  Скачать: https://visualstudio.microsoft.com/downloads/
  → "Tools for Visual Studio" → "Build Tools for Visual Studio"
  При установке выбрать: "Desktop development with C++"

### Сборка в VSCode
1. Открой папку в VSCode
2. Открой терминал: Terminal → New Terminal
3. Выполни команду для активации компилятора:
   ```
   "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
   ```
   (путь может отличаться, найди vcvars64.bat через поиск в C:\Program Files)
4. Нажми Ctrl+Shift+B → "Собрать NetBackup"
5. Появится NetBackup.exe — запускай!

### Альтернатива — сборка из командной строки
Открой "x64 Native Tools Command Prompt for VS" из Пуска и выполни:
```
cd путь\до\netbackup-mvp
cl /EHsc /std:c++17 /Fe:NetBackup.exe src\MainWindow.cpp /I include /link comctl32.lib shell32.lib ole32.lib /SUBSYSTEM:WINDOWS
```

## Что умеет MVP

- Выбор папки для слежки
- Выбор папки для бэкапа
- Автоматическое копирование при изменении файлов
- Сохранение структуры подпапок
- Журнал событий с временем
- Статистика: файлов скопировано, ошибки, трафик
- Настройки сохраняются между запусками (%APPDATA%\NetBackup\config.ini)
- Лог файл (%APPDATA%\NetBackup\backup.log)
- Кнопка открыть папку бэкапа

## Следующий шаг — Google Drive

После того как локальный бэкап работает, добавим загрузку в Google Drive:
- Регистрация приложения в Google Cloud Console
- OAuth2 авторизация
- Загрузка через Google Drive REST API
