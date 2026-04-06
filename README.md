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

### Сборка в Visual Studio
---
Для сборки в VS должен быть установлен "Developing Desktop Apps C++"

После этого зайдите в **Developer Command Prompt for VS** и для сборки текущей версии приложения выполните:
```
cd C:\Users\путь\к\вашей\папке
cl /EHsc /std:c++17 /utf-8 /Fe:NetBackup.exe src\MainWindow.cpp /I include /link comctl32.lib shell32.lib ole32.lib user32.lib gdi32.lib /SUBSYSTEM:WINDOWS
```

---


### Альтернатива - сборка из командной строки
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

## next feat::

После того как локальный бэкап работает, добавим загрузку в Google Drive:
- Регистрация приложения в Google Cloud Console
- OAuth2 авторизация
- Загрузка через Google Drive REST API
