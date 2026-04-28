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

### Сборка проекта

#### Быстрая сборка (рекомендуется) ⚡
Просто запустите:
```
build.bat
```
Скрипт автоматически:
- Инициализирует окружение Visual Studio
- Скомпилирует проект
- Создаст `NetBackup.exe`

---

#### Ручная сборка через Developer Command Prompt
Для сборки в VS должен быть установлен "Developing Desktop Apps C++"

Зайдите в **Developer Command Prompt for VS** и выполните:
```
cd C:\Users\путь\к\вашей\папке
cl /EHsc /std:c++17 /utf-8 /Fe:NetBackup.exe src\MainWindow.cpp /I include /link comctl32.lib shell32.lib ole32.lib user32.lib gdi32.lib /SUBSYSTEM:WINDOWS
```

---

#### Альтернатива - сборка из x64 Native Tools Command Prompt
Открой "x64 Native Tools Command Prompt for VS" из Пуска и выполни:
```
cd путь\до\netbackup-mvp
cl /EHsc /std:c++17 /utf-8 /Fe:NetBackup.exe src\MainWindow.cpp /I include /link comctl32.lib shell32.lib ole32.lib user32.lib gdi32.lib /SUBSYSTEM:WINDOWS
```

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
