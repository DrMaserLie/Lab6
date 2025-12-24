# ⏳ Temporium - СУБД Компьютерные Игры

Курсовая работа по дисциплине «Программирование»  
**ФГБОУ ВО НГТУ**, Кафедра «Защита информации»

## Описание

Temporium — локальная СУБД для управления персональной библиотекой компьютерных игр с поддержкой:
- Многопользовательского режима с разделением прав
- Гибкой многокритериальной фильтрации
- Системы оценок, тегов и заметок
- Экспорта/импорта с SHA-256 защитой целостности
- Защиты от SQL-инъекций

## Структура базы данных (5 таблиц для ЛР6)

```
┌─────────────┐       ┌─────────────┐
│   users     │       │   genres    │
├─────────────┤       ├─────────────┤
│ id (PK)     │       │ id (PK)     │
│ username    │       │ name        │
│ password_hash│      │ description │
│ is_admin    │       └──────┬──────┘
│ created_at  │              │
└──────┬──────┘              │
       │                     │
       │ 1:N            N:1  │
       ▼                     ▼
┌──────────────────────────────────┐
│              games               │
├──────────────────────────────────┤
│ id (PK)                          │
│ name                             │
│ disk_space, ram_usage, vram_required│
│ genre_id (FK → genres.id)        │
│ completed, rating, is_favorite   │
│ is_installed, notes, url         │
│ user_id (FK → users.id)          │
│ created_at                       │
└─────────────┬────────────────────┘
              │
              │ N:M
              ▼
┌─────────────────────────────────┐
│          game_tags              │
├─────────────────────────────────┤
│ id (PK)                         │
│ game_id (FK → games.id)         │
│ tag_id (FK → tags.id)           │
└─────────────┬───────────────────┘
              │
              │ N:1
              ▼
┌─────────────────────────────────┐
│             tags                │
├─────────────────────────────────┤
│ id (PK)                         │
│ name                            │
│ user_id (FK → users.id)         │
│ color                           │
└─────────────────────────────────┘
```

## Демонстрационные SQL-запросы

См. файл `sql/init.sql` для 10 примеров запросов с:
- LEFT JOIN, INNER JOIN
- GROUP BY, HAVING
- COUNT, AVG, SUM
- ORDER BY, LIMIT
- LIKE (ILIKE)
- STRING_AGG
- EXISTS, подзапросы

## Технологии

- **Язык:** C++17
- **GUI:** Qt 6
- **СУБД:** PostgreSQL (в Docker)
- **Библиотека БД:** libpqxx
- **Контейнеризация:** Docker, Docker Compose

## Требования

- Linux (Ubuntu 22.04+)
- Docker и Docker Compose
- Qt 6 (qt6-base-dev)
- libpqxx-dev
- g++ с поддержкой C++17

## Установка и запуск

```bash
# Клонировать репозиторий
git clone <url>
cd temporium

# Запустить (автоматически соберёт и запустит Docker)
./run.sh
```

## Учётные данные по умолчанию

- **Администратор:** admin / admin123

## Структура проекта

```
temporium/
├── include/
│   ├── database_manager.h  # Работа с PostgreSQL
│   ├── mainwindow.h        # GUI
│   ├── types.h             # Структуры данных
│   └── hash_utils.h        # SHA-256 хеширование
├── src/
│   ├── main.cpp
│   ├── database_manager.cpp
│   └── mainwindow.cpp
├── sql/
│   └── init.sql            # Схема БД и примеры запросов
├── docker/
│   └── docker-compose.yml
├── resources/
│   └── temporium.svg       # Иконка приложения
├── CMakeLists.txt
└── run.sh                  # Скрипт запуска
```

## Защита информации

1. **SQL-инъекции:** Параметризованные запросы (prepared statements)
2. **Пароли:** SHA-256 хеширование с солью
3. **Экспорт/импорт:** SHA-256 подпись файлов
4. **Разграничение доступа:** Игры привязаны к user_id

## Лицензия

MIT
