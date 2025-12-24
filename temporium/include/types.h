#ifndef TYPES_H
#define TYPES_H

#include <string>
#include <vector>
#include <cstdint>
#include <cstring>

namespace Temporium {

// Максимальные значения для полей (единые для всего приложения)
constexpr double MAX_DISK_SPACE = 500.0;    // ГБ
constexpr double MAX_RAM_USAGE = 128.0;      // ГБ
constexpr double MAX_VRAM_REQUIRED = 48.0;   // ГБ

constexpr double MIN_DISK_SPACE = 0.1;
constexpr double MIN_RAM_USAGE = 0.5;
constexpr double MIN_VRAM_REQUIRED = 0.5;

// ============================================================
// ТАБЛИЦА 1: users - Пользователи системы
// ============================================================
struct User {
    int id;
    std::string username;
    std::string password_hash;
    bool is_admin;              // Флаг администратора
    std::string created_at;
    
    User() : id(0), is_admin(false) {}
};

// ============================================================
// ТАБЛИЦА 2: genres - Справочник жанров игр
// ============================================================
struct Genre {
    int id;
    std::string name;           // Название жанра
    std::string description;    // Описание жанра
    
    Genre() : id(0) {}
    Genre(int _id, const std::string& _name, const std::string& _desc = "") 
        : id(_id), name(_name), description(_desc) {}
};

// ============================================================
// ТАБЛИЦА 3: tags - Пользовательские теги
// ============================================================
struct Tag {
    int id;
    std::string name;           // Название тега
    int user_id;                // Владелец тега (FK -> users.id)
    std::string color;          // Цвет тега (hex)
    
    Tag() : id(0), user_id(0) {}
    Tag(int _id, const std::string& _name, int _user_id, const std::string& _color = "#808080")
        : id(_id), name(_name), user_id(_user_id), color(_color) {}
};

// ============================================================
// ТАБЛИЦА 4: games - Компьютерные игры
// ============================================================
struct Game {
    int id;
    std::string name;           // Название игры
    double disk_space;          // Место на диске (ГБ)
    double ram_usage;           // Потребление ОЗУ (ГБ)
    double vram_required;       // Требуемая видеопамять (ГБ)
    int genre_id;               // ID жанра (FK -> genres.id)
    std::string genre;          // Название жанра (для отображения, JOIN)
    bool completed;             // Пройдено (да/нет)
    std::string url;            // Ссылка на игру (Steam, GOG и т.д.)
    int user_id;                // ID пользователя-владельца (FK -> users.id)
    int rating;                 // Оценка: -1 = отсутствует, 0-10 = оценка
    bool is_favorite;           // Избранное
    bool is_installed;          // Установлено
    std::string notes;          // Заметки пользователя
    std::string tags;           // Теги (строка для отображения, агрегация из game_tags)
    std::vector<int> tag_ids;   // ID тегов (для редактирования)
    
    Game() : id(0), disk_space(0), ram_usage(0), vram_required(0), genre_id(0),
             completed(false), user_id(0), rating(-1), is_favorite(false), is_installed(false) {}
};

// ============================================================
// ТАБЛИЦА 5: game_tags - Связь игр и тегов (Many-to-Many)
// ============================================================
struct GameTag {
    int id;
    int game_id;                // FK -> games.id
    int tag_id;                 // FK -> tags.id
    
    GameTag() : id(0), game_id(0), tag_id(0) {}
    GameTag(int _game_id, int _tag_id) : id(0), game_id(_game_id), tag_id(_tag_id) {}
};

// Структура фильтра для поиска игр
struct GameFilter {
    bool filter_completed;
    bool completed_value;
    
    bool filter_genre;
    int genre_id;               // Фильтр по ID жанра
    
    bool filter_disk_space_min;
    double disk_space_min;
    
    bool filter_disk_space_max;
    double disk_space_max;
    
    bool filter_ram_min;
    double ram_min;
    
    bool filter_ram_max;
    double ram_max;
    
    bool filter_vram_min;
    double vram_min;
    
    bool filter_vram_max;
    double vram_max;
    
    // Фильтр по тегам
    bool filter_tag;
    int tag_id;                 // Фильтр по ID тега
    
    bool filter_favorite;
    bool favorite_value;
    
    bool filter_installed;
    bool installed_value;
    
    bool filter_rating_min;
    int rating_min;
    
    bool filter_rating_max;
    int rating_max;
    
    bool filter_has_rating;      // Фильтр: только с оценкой / без оценки
    bool has_rating_value;
    
    GameFilter() : 
        filter_completed(false), completed_value(false),
        filter_genre(false), genre_id(0),
        filter_disk_space_min(false), disk_space_min(0),
        filter_disk_space_max(false), disk_space_max(0),
        filter_ram_min(false), ram_min(0),
        filter_ram_max(false), ram_max(0),
        filter_vram_min(false), vram_min(0),
        filter_vram_max(false), vram_max(0),
        filter_tag(false), tag_id(0),
        filter_favorite(false), favorite_value(false),
        filter_installed(false), installed_value(false),
        filter_rating_min(false), rating_min(0),
        filter_rating_max(false), rating_max(10),
        filter_has_rating(false), has_rating_value(false) {}
    
    void reset() {
        filter_completed = false;
        filter_genre = false;
        genre_id = 0;
        filter_disk_space_min = false;
        filter_disk_space_max = false;
        filter_ram_min = false;
        filter_ram_max = false;
        filter_vram_min = false;
        filter_vram_max = false;
        filter_tag = false;
        tag_id = 0;
        filter_favorite = false;
        filter_installed = false;
        filter_rating_min = false;
        filter_rating_max = false;
        filter_has_rating = false;
    }
};

// Магическое число для идентификации файла Temporium
constexpr uint32_t FILE_MAGIC = 0x54454D50; // "TEMP" в hex

// Версия формата файла (увеличена для новых полей)
constexpr uint16_t FILE_VERSION = 4;  // v4 для нормализованной БД

// Заголовок бинарного файла с хешем для проверки целостности
#pragma pack(push, 1)
struct BinaryFileHeader {
    uint32_t magic;              // Магическое число для идентификации
    uint16_t version;            // Версия формата
    uint32_t record_count;       // Количество записей
    char hash[64];               // SHA-256 хеш данных (hex-строка)
    uint8_t reserved[26];        // Резерв для будущих расширений
    
    BinaryFileHeader() : magic(FILE_MAGIC), version(FILE_VERSION), record_count(0) {
        std::memset(hash, 0, sizeof(hash));
        std::memset(reserved, 0, sizeof(reserved));
    }
};
#pragma pack(pop)

// Структура для бинарного файла (одна запись игры)
#pragma pack(push, 1)
struct BinaryGameRecord {
    int32_t id;
    char name[256];
    double disk_space;
    double ram_usage;
    double vram_required;
    int32_t genre_id;            // ID жанра
    char genre[64];              // Название жанра (для совместимости)
    uint8_t completed;
    char url[512];              // URL ссылка
    int32_t user_id;
    int32_t rating;             // -1 = отсутствует, 0-10 = оценка
    uint8_t is_favorite;        // Избранное
    uint8_t is_installed;       // Установлено
    char notes[1024];           // Заметки
    char tags[256];             // Теги (строка для совместимости)
    
    BinaryGameRecord() : id(0), disk_space(0), ram_usage(0), 
                         vram_required(0), genre_id(0), completed(0), user_id(0),
                         rating(-1), is_favorite(0), is_installed(0) {
        std::memset(name, 0, sizeof(name));
        std::memset(genre, 0, sizeof(genre));
        std::memset(url, 0, sizeof(url));
        std::memset(notes, 0, sizeof(notes));
        std::memset(tags, 0, sizeof(tags));
    }
};
#pragma pack(pop)

// Предустановленные жанры для заполнения справочника
const std::vector<std::pair<std::string, std::string>> DEFAULT_GENRES = {
    {"Action", "Экшен игры с акцентом на боевую систему"},
    {"Adventure", "Приключенческие игры с исследованием и головоломками"},
    {"RPG", "Ролевые игры с развитием персонажа"},
    {"Strategy", "Стратегические игры с тактическим планированием"},
    {"Simulation", "Симуляторы реальных процессов"},
    {"Sports", "Спортивные игры"},
    {"Racing", "Гоночные игры"},
    {"Puzzle", "Головоломки и логические игры"},
    {"Horror", "Хоррор игры"},
    {"Shooter", "Шутеры от первого/третьего лица"},
    {"Fighting", "Файтинги"},
    {"Platformer", "Платформеры"},
    {"Sandbox", "Песочницы с открытым миром"},
    {"MMO", "Многопользовательские онлайн-игры"},
    {"Visual Novel", "Визуальные новеллы"},
    {"Other", "Другие жанры"}
};

// Список названий жанров (для совместимости с UI)
const std::vector<std::string> GENRES = {
    "Action", "Adventure", "RPG", "Strategy", "Simulation",
    "Sports", "Racing", "Puzzle", "Horror", "Shooter",
    "Fighting", "Platformer", "Sandbox", "MMO", "Visual Novel", "Other"
};

} // namespace Temporium

#endif // TYPES_H
