-- ============================================================
-- Temporium Database Schema
-- 5 таблиц для ЛР6
-- ============================================================

-- ТАБЛИЦА 1: users - Пользователи системы
CREATE TABLE IF NOT EXISTS users (
    id SERIAL PRIMARY KEY,
    username VARCHAR(255) UNIQUE NOT NULL,
    password_hash VARCHAR(64) NOT NULL,
    is_admin BOOLEAN DEFAULT FALSE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- ТАБЛИЦА 2: genres - Справочник жанров игр
CREATE TABLE IF NOT EXISTS genres (
    id SERIAL PRIMARY KEY,
    name VARCHAR(64) UNIQUE NOT NULL,
    description TEXT DEFAULT ''
);

-- ТАБЛИЦА 3: tags - Пользовательские теги
CREATE TABLE IF NOT EXISTS tags (
    id SERIAL PRIMARY KEY,
    name VARCHAR(64) NOT NULL,
    user_id INTEGER REFERENCES users(id) ON DELETE CASCADE,
    color VARCHAR(7) DEFAULT '#808080',
    UNIQUE(name, user_id)
);

-- ТАБЛИЦА 4: games - Компьютерные игры
CREATE TABLE IF NOT EXISTS games (
    id SERIAL PRIMARY KEY,
    name VARCHAR(255) NOT NULL,
    disk_space DOUBLE PRECISION NOT NULL,
    ram_usage DOUBLE PRECISION NOT NULL,
    vram_required DOUBLE PRECISION NOT NULL,
    genre_id INTEGER REFERENCES genres(id) ON DELETE SET NULL,
    completed BOOLEAN DEFAULT FALSE,
    url VARCHAR(512) DEFAULT '',
    user_id INTEGER REFERENCES users(id) ON DELETE CASCADE,
    rating INTEGER DEFAULT -1,
    is_favorite BOOLEAN DEFAULT FALSE,
    is_installed BOOLEAN DEFAULT FALSE,
    notes TEXT DEFAULT '',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(name, user_id)
);

-- ТАБЛИЦА 5: game_tags - Связь игр и тегов (Many-to-Many)
CREATE TABLE IF NOT EXISTS game_tags (
    id SERIAL PRIMARY KEY,
    game_id INTEGER REFERENCES games(id) ON DELETE CASCADE,
    tag_id INTEGER REFERENCES tags(id) ON DELETE CASCADE,
    UNIQUE(game_id, tag_id)
);

-- Индексы для оптимизации запросов
CREATE INDEX IF NOT EXISTS idx_games_user_id ON games(user_id);
CREATE INDEX IF NOT EXISTS idx_games_genre_id ON games(genre_id);
CREATE INDEX IF NOT EXISTS idx_games_completed ON games(completed);
CREATE INDEX IF NOT EXISTS idx_games_favorite ON games(is_favorite);
CREATE INDEX IF NOT EXISTS idx_games_rating ON games(rating);
CREATE INDEX IF NOT EXISTS idx_games_installed ON games(is_installed);
CREATE INDEX IF NOT EXISTS idx_tags_user_id ON tags(user_id);
CREATE INDEX IF NOT EXISTS idx_game_tags_game_id ON game_tags(game_id);
CREATE INDEX IF NOT EXISTS idx_game_tags_tag_id ON game_tags(tag_id);

-- Предустановленные жанры
INSERT INTO genres (name, description) VALUES
    ('Action', 'Экшен игры с акцентом на боевую систему'),
    ('Adventure', 'Приключенческие игры с исследованием'),
    ('RPG', 'Ролевые игры с развитием персонажа'),
    ('Strategy', 'Стратегические игры'),
    ('Simulation', 'Симуляторы реальных процессов'),
    ('Sports', 'Спортивные игры'),
    ('Racing', 'Гоночные игры'),
    ('Puzzle', 'Головоломки'),
    ('Horror', 'Хоррор игры'),
    ('Shooter', 'Шутеры'),
    ('Fighting', 'Файтинги'),
    ('Platformer', 'Платформеры'),
    ('Sandbox', 'Песочницы'),
    ('MMO', 'Многопользовательские онлайн-игры'),
    ('Visual Novel', 'Визуальные новеллы'),
    ('Other', 'Другие жанры')
ON CONFLICT (name) DO NOTHING;

SELECT 'База данных Temporium (5 таблиц) готова!' AS status;


-- получить все игры пользователя
-- SELECT * FROM games WHERE user_id = 1;

-- получить игры с названиями жанров
-- SELECT g.*, gen.name as genre_name 
-- FROM games g 
-- LEFT JOIN genres gen ON g.genre_id = gen.id 
-- WHERE g.user_id = 1;

-- получить теги игры
-- SELECT t.* FROM tags t 
-- INNER JOIN game_tags gt ON t.id = gt.tag_id 
-- WHERE gt.game_id = 1;

-- GROUP BY с агрегатными функциями (COUNT, AVG, SUM)
-- SELECT gen.name, COUNT(g.id) as games_count, 
--        AVG(g.rating) as avg_rating, 
--        SUM(g.disk_space) as total_space
-- FROM genres gen
-- LEFT JOIN games g ON gen.id = g.genre_id AND g.user_id = 1
-- GROUP BY gen.id, gen.name
-- HAVING COUNT(g.id) > 0;

-- топ игр по рейтингу
-- SELECT * FROM games 
-- WHERE user_id = 1 AND rating >= 0 
-- ORDER BY rating DESC 
-- LIMIT 10;

-- поиск по названию
-- SELECT * FROM games 
-- WHERE user_id = 1 AND name ILIKE '%witcher%';

-- агрегация тегов в строку
-- SELECT STRING_AGG(t.name, ', ' ORDER BY t.name) as tags
-- FROM tags t 
-- INNER JOIN game_tags gt ON t.id = gt.tag_id 
-- WHERE gt.game_id = 1;

-- фильтрация по наличию тега
-- SELECT g.* FROM games g 
-- WHERE g.user_id = 1 AND EXISTS (
--     SELECT 1 FROM game_tags gt WHERE gt.game_id = g.id AND gt.tag_id = 1
-- );

-- подзапрос с HAVING - непройденные игры из высокорейтинговых жанров
-- SELECT g.* FROM games g 
-- WHERE g.user_id = 1 AND g.completed = FALSE AND g.genre_id IN (
--     SELECT genre_id FROM games 
--     WHERE user_id = 1 AND rating >= 0 
--     GROUP BY genre_id 
--     HAVING AVG(rating) >= 7
-- );

-- несколько JOIN и GROUP BY
-- SELECT g.id, g.name, gen.name as genre, 
--        STRING_AGG(t.name, ', ' ORDER BY t.name) as tags,
--        g.rating, g.completed
-- FROM games g
-- LEFT JOIN genres gen ON g.genre_id = gen.id
-- LEFT JOIN game_tags gt ON g.id = gt.game_id
-- LEFT JOIN tags t ON gt.tag_id = t.id
-- WHERE g.user_id = 1
-- GROUP BY g.id, gen.name
-- ORDER BY g.name;
