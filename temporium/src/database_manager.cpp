#include "database_manager.h"
#include "hash_utils.h"
#include <fstream>
#include <cstring>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <set>
namespace Temporium {
DatabaseManager::DatabaseManager() : conn_(nullptr) {}
DatabaseManager::~DatabaseManager() {
    disconnect();
}
bool DatabaseManager::connect(const std::string& host, int port,
                              const std::string& dbname,
                              const std::string& user,
                              const std::string& password) {
    try {
        std::stringstream conn_str;
        conn_str << "host=" << host 
                 << " port=" << port 
                 << " dbname=" << dbname 
                 << " user=" << user 
                 << " password=" << password;
        conn_ = std::make_unique<pqxx::connection>(conn_str.str());
        if (conn_->is_open()) {
            if (initializeTables()) {
                ensureAdminExists();
                ensureDefaultGenres();
                return true;
            }
        }
        last_error_ = "Failed to open database connection";
        return false;
    } catch (const std::exception& e) {
        last_error_ = std::string("Connection error: ") + e.what();
        return false;
    }
}
void DatabaseManager::disconnect() {
    if (conn_) {
        conn_.reset();
    }
}
bool DatabaseManager::isConnected() const {
    return conn_ && conn_->is_open();
}
bool DatabaseManager::initializeTables() {
    try {
        pqxx::work txn(*conn_);
        txn.exec(
            "CREATE TABLE IF NOT EXISTS users ("
            "    id SERIAL PRIMARY KEY,"
            "    username VARCHAR(255) UNIQUE NOT NULL,"
            "    password_hash VARCHAR(64) NOT NULL,"
            "    is_admin BOOLEAN DEFAULT FALSE,"
            "    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
            ")"
        );
        txn.exec(
            "CREATE TABLE IF NOT EXISTS genres ("
            "    id SERIAL PRIMARY KEY,"
            "    name VARCHAR(64) UNIQUE NOT NULL,"
            "    description TEXT DEFAULT ''"
            ")"
        );
        txn.exec(
            "CREATE TABLE IF NOT EXISTS tags ("
            "    id SERIAL PRIMARY KEY,"
            "    name VARCHAR(64) NOT NULL,"
            "    user_id INTEGER REFERENCES users(id) ON DELETE CASCADE,"
            "    color VARCHAR(7) DEFAULT '#808080',"
            "    UNIQUE(name, user_id)"
            ")"
        );
        txn.exec(
            "CREATE TABLE IF NOT EXISTS games ("
            "    id SERIAL PRIMARY KEY,"
            "    name VARCHAR(255) NOT NULL,"
            "    disk_space DOUBLE PRECISION NOT NULL,"
            "    ram_usage DOUBLE PRECISION NOT NULL,"
            "    vram_required DOUBLE PRECISION NOT NULL,"
            "    genre_id INTEGER REFERENCES genres(id) ON DELETE SET NULL,"
            "    completed BOOLEAN DEFAULT FALSE,"
            "    url VARCHAR(512) DEFAULT '',"
            "    user_id INTEGER REFERENCES users(id) ON DELETE CASCADE,"
            "    rating INTEGER DEFAULT -1,"
            "    is_favorite BOOLEAN DEFAULT FALSE,"
            "    is_installed BOOLEAN DEFAULT FALSE,"
            "    notes TEXT DEFAULT '',"
            "    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
            "    UNIQUE(name, user_id)"
            ")"
        );
        txn.exec(
            "CREATE TABLE IF NOT EXISTS game_tags ("
            "    id SERIAL PRIMARY KEY,"
            "    game_id INTEGER REFERENCES games(id) ON DELETE CASCADE,"
            "    tag_id INTEGER REFERENCES tags(id) ON DELETE CASCADE,"
            "    UNIQUE(game_id, tag_id)"
            ")"
        );
        txn.exec(
            "DO $$ BEGIN "
            "    ALTER TABLE users ADD COLUMN IF NOT EXISTS is_admin BOOLEAN DEFAULT FALSE; "
            "EXCEPTION WHEN others THEN NULL; END $$"
        );
        txn.exec(
            "DO $$ BEGIN "
            "    ALTER TABLE games ADD COLUMN IF NOT EXISTS genre_id INTEGER REFERENCES genres(id) ON DELETE SET NULL; "
            "EXCEPTION WHEN others THEN NULL; END $$"
        );
        txn.exec("CREATE INDEX IF NOT EXISTS idx_games_user_id ON games(user_id)");
        txn.exec("CREATE INDEX IF NOT EXISTS idx_games_genre_id ON games(genre_id)");
        txn.exec("CREATE INDEX IF NOT EXISTS idx_games_completed ON games(completed)");
        txn.exec("CREATE INDEX IF NOT EXISTS idx_games_favorite ON games(is_favorite)");
        txn.exec("CREATE INDEX IF NOT EXISTS idx_games_rating ON games(rating)");
        txn.exec("CREATE INDEX IF NOT EXISTS idx_games_installed ON games(is_installed)");
        txn.exec("CREATE INDEX IF NOT EXISTS idx_tags_user_id ON tags(user_id)");
        txn.exec("CREATE INDEX IF NOT EXISTS idx_game_tags_game_id ON game_tags(game_id)");
        txn.exec("CREATE INDEX IF NOT EXISTS idx_game_tags_tag_id ON game_tags(tag_id)");
        txn.commit();
        return true;
    } catch (const std::exception& e) {
        last_error_ = std::string("Table initialization error: ") + e.what();
        return false;
    }
}
void DatabaseManager::ensureAdminExists() {
    try {
        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec("SELECT COUNT(*) FROM users WHERE is_admin = TRUE");
        if (r[0][0].as<int>() == 0) {
            std::string adminHash = HashUtils::hashPassword("admin123", "admin");
            txn.exec_params(
                "INSERT INTO users (username, password_hash, is_admin) VALUES ($1, $2, TRUE)",
                "admin", adminHash
            );
        }
        txn.commit();
    } catch (const std::exception& e) {
    }
}
void DatabaseManager::ensureDefaultGenres() {
    try {
        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec("SELECT COUNT(*) FROM genres");
        if (r[0][0].as<int>() == 0) {
            for (const auto& genre : DEFAULT_GENRES) {
                txn.exec_params(
                    "INSERT INTO genres (name, description) VALUES ($1, $2) ON CONFLICT (name) DO NOTHING",
                    genre.first, genre.second
                );
            }
        }
        txn.commit();
    } catch (const std::exception& e) {
    }
}
bool DatabaseManager::registerUser(const std::string& username, const std::string& password_hash, bool is_admin) {
    try {
        pqxx::work txn(*conn_);
        txn.exec_params(
            "INSERT INTO users (username, password_hash, is_admin) VALUES ($1, $2, $3)",
            username, password_hash, is_admin
        );
        txn.commit();
        return true;
    } catch (const std::exception& e) {
        last_error_ = std::string("Registration error: ") + e.what();
        return false;
    }
}
User DatabaseManager::authenticateUser(const std::string& username, const std::string& password_hash) {
    User user;
    try {
        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec_params(
            "SELECT id, username, password_hash, is_admin FROM users WHERE username = $1 AND password_hash = $2",
            username, password_hash
        );
        if (!r.empty()) {
            user.id = r[0]["id"].as<int>();
            user.username = r[0]["username"].as<std::string>();
            user.password_hash = r[0]["password_hash"].as<std::string>();
            user.is_admin = r[0]["is_admin"].as<bool>();
        }
        txn.commit();
    } catch (const std::exception& e) {
        last_error_ = std::string("Authentication error: ") + e.what();
    }
    return user;
}
bool DatabaseManager::userExists(const std::string& username) {
    try {
        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec_params(
            "SELECT COUNT(*) FROM users WHERE username = $1",
            username
        );
        txn.commit();
        return r[0][0].as<int>() > 0;
    } catch (const std::exception& e) {
        last_error_ = std::string("User check error: ") + e.what();
        return false;
    }
}
std::vector<User> DatabaseManager::getAllUsers() {
    std::vector<User> users;
    try {
        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec(
            "SELECT id, username, password_hash, is_admin FROM users ORDER BY username"
        );
        for (const auto& row : r) {
            User user;
            user.id = row["id"].as<int>();
            user.username = row["username"].as<std::string>();
            user.password_hash = row["password_hash"].as<std::string>();
            user.is_admin = row["is_admin"].as<bool>();
            users.push_back(user);
        }
        txn.commit();
    } catch (const std::exception& e) {
        last_error_ = std::string("Get users error: ") + e.what();
    }
    return users;
}
bool DatabaseManager::deleteUser(int user_id) {
    try {
        pqxx::work txn(*conn_);
        txn.exec_params("DELETE FROM users WHERE id = $1 AND is_admin = FALSE", user_id);
        txn.commit();
        return true;
    } catch (const std::exception& e) {
        last_error_ = std::string("Delete user error: ") + e.what();
        return false;
    }
}
bool DatabaseManager::isAdmin(int user_id) {
    try {
        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec_params(
            "SELECT is_admin FROM users WHERE id = $1",
            user_id
        );
        txn.commit();
        return !r.empty() && r[0][0].as<bool>();
    } catch (const std::exception& e) {
        return false;
    }
}
int DatabaseManager::getUserGamesCount(int user_id) {
    try {
        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec_params(
            "SELECT COUNT(*) FROM games WHERE user_id = $1",
            user_id
        );
        txn.commit();
        return r[0][0].as<int>();
    } catch (const std::exception& e) {
        return 0;
    }
}
bool DatabaseManager::changeUsername(int user_id, const std::string& new_username, const std::string& current_password) {
    try {
        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec_params(
            "SELECT password_hash FROM users WHERE id = $1",
            user_id
        );
        if (r.empty() || r[0][0].as<std::string>() != current_password) {
            last_error_ = "Invalid current password";
            return false;
        }
        txn.exec_params(
            "UPDATE users SET username = $1 WHERE id = $2",
            new_username, user_id
        );
        txn.commit();
        return true;
    } catch (const std::exception& e) {
        last_error_ = std::string("Change username error: ") + e.what();
        return false;
    }
}
bool DatabaseManager::changePassword(int user_id, const std::string& new_password_hash) {
    try {
        pqxx::work txn(*conn_);
        txn.exec_params(
            "UPDATE users SET password_hash = $1 WHERE id = $2",
            new_password_hash, user_id
        );
        txn.commit();
        return true;
    } catch (const std::exception& e) {
        last_error_ = std::string("Change password error: ") + e.what();
        return false;
    }
}
bool DatabaseManager::resetAdminCredentials() {
    try {
        pqxx::work txn(*conn_);
        std::string newHash = HashUtils::hashPassword("admin123", "admin");
        txn.exec_params(
            "UPDATE users SET username = 'admin', password_hash = $1 WHERE is_admin = TRUE",
            newHash
        );
        txn.commit();
        return true;
    } catch (const std::exception& e) {
        last_error_ = std::string("Reset admin error: ") + e.what();
        return false;
    }
}
std::vector<Genre> DatabaseManager::getAllGenres() {
    std::vector<Genre> genres;
    try {
        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec("SELECT id, name, description FROM genres ORDER BY name");
        for (const auto& row : r) {
            Genre genre;
            genre.id = row["id"].as<int>();
            genre.name = row["name"].as<std::string>();
            genre.description = row["description"].is_null() ? "" : row["description"].as<std::string>();
            genres.push_back(genre);
        }
        txn.commit();
    } catch (const std::exception& e) {
        last_error_ = std::string("Get genres error: ") + e.what();
    }
    return genres;
}
Genre DatabaseManager::getGenreById(int genre_id) {
    Genre genre;
    try {
        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec_params(
            "SELECT id, name, description FROM genres WHERE id = $1",
            genre_id
        );
        if (!r.empty()) {
            genre.id = r[0]["id"].as<int>();
            genre.name = r[0]["name"].as<std::string>();
            genre.description = r[0]["description"].is_null() ? "" : r[0]["description"].as<std::string>();
        }
        txn.commit();
    } catch (const std::exception& e) {
        last_error_ = std::string("Get genre error: ") + e.what();
    }
    return genre;
}
Genre DatabaseManager::getGenreByName(const std::string& name) {
    Genre genre;
    try {
        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec_params(
            "SELECT id, name, description FROM genres WHERE name = $1",
            name
        );
        if (!r.empty()) {
            genre.id = r[0]["id"].as<int>();
            genre.name = r[0]["name"].as<std::string>();
            genre.description = r[0]["description"].is_null() ? "" : r[0]["description"].as<std::string>();
        }
        txn.commit();
    } catch (const std::exception& e) {
        last_error_ = std::string("Get genre by name error: ") + e.what();
    }
    return genre;
}
int DatabaseManager::addGenre(const std::string& name, const std::string& description) {
    try {
        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec_params(
            "INSERT INTO genres (name, description) VALUES ($1, $2) RETURNING id",
            name, description
        );
        txn.commit();
        return r[0][0].as<int>();
    } catch (const std::exception& e) {
        last_error_ = std::string("Add genre error: ") + e.what();
        return -1;
    }
}
bool DatabaseManager::updateGenre(int genre_id, const std::string& name, const std::string& description) {
    try {
        pqxx::work txn(*conn_);
        txn.exec_params(
            "UPDATE genres SET name = $1, description = $2 WHERE id = $3",
            name, description, genre_id
        );
        txn.commit();
        return true;
    } catch (const std::exception& e) {
        last_error_ = std::string("Update genre error: ") + e.what();
        return false;
    }
}
bool DatabaseManager::deleteGenre(int genre_id) {
    try {
        pqxx::work txn(*conn_);
        txn.exec_params("DELETE FROM genres WHERE id = $1", genre_id);
        txn.commit();
        return true;
    } catch (const std::exception& e) {
        last_error_ = std::string("Delete genre error: ") + e.what();
        return false;
    }
}
std::vector<Tag> DatabaseManager::getUserTags(int user_id) {
    std::vector<Tag> tags;
    try {
        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec_params(
            "SELECT id, name, user_id, color FROM tags WHERE user_id = $1 ORDER BY name",
            user_id
        );
        for (const auto& row : r) {
            Tag tag;
            tag.id = row["id"].as<int>();
            tag.name = row["name"].as<std::string>();
            tag.user_id = row["user_id"].as<int>();
            tag.color = row["color"].is_null() ? "#808080" : row["color"].as<std::string>();
            tags.push_back(tag);
        }
        txn.commit();
    } catch (const std::exception& e) {
        last_error_ = std::string("Get user tags error: ") + e.what();
    }
    return tags;
}
Tag DatabaseManager::getTagById(int tag_id) {
    Tag tag;
    try {
        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec_params(
            "SELECT id, name, user_id, color FROM tags WHERE id = $1",
            tag_id
        );
        if (!r.empty()) {
            tag.id = r[0]["id"].as<int>();
            tag.name = r[0]["name"].as<std::string>();
            tag.user_id = r[0]["user_id"].as<int>();
            tag.color = r[0]["color"].is_null() ? "#808080" : r[0]["color"].as<std::string>();
        }
        txn.commit();
    } catch (const std::exception& e) {
        last_error_ = std::string("Get tag error: ") + e.what();
    }
    return tag;
}
Tag DatabaseManager::getTagByName(const std::string& name, int user_id) {
    Tag tag;
    try {
        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec_params(
            "SELECT id, name, user_id, color FROM tags WHERE name = $1 AND user_id = $2",
            name, user_id
        );
        if (!r.empty()) {
            tag.id = r[0]["id"].as<int>();
            tag.name = r[0]["name"].as<std::string>();
            tag.user_id = r[0]["user_id"].as<int>();
            tag.color = r[0]["color"].is_null() ? "#808080" : r[0]["color"].as<std::string>();
        }
        txn.commit();
    } catch (const std::exception& e) {
        last_error_ = std::string("Get tag by name error: ") + e.what();
    }
    return tag;
}
int DatabaseManager::addTag(const std::string& name, int user_id, const std::string& color) {
    try {
        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec_params(
            "INSERT INTO tags (name, user_id, color) VALUES ($1, $2, $3) RETURNING id",
            name, user_id, color
        );
        txn.commit();
        return r[0][0].as<int>();
    } catch (const std::exception& e) {
        last_error_ = std::string("Add tag error: ") + e.what();
        return -1;
    }
}
bool DatabaseManager::updateTag(int tag_id, const std::string& name, const std::string& color) {
    try {
        pqxx::work txn(*conn_);
        txn.exec_params(
            "UPDATE tags SET name = $1, color = $2 WHERE id = $3",
            name, color, tag_id
        );
        txn.commit();
        return true;
    } catch (const std::exception& e) {
        last_error_ = std::string("Update tag error: ") + e.what();
        return false;
    }
}
bool DatabaseManager::deleteTag(int tag_id) {
    try {
        pqxx::work txn(*conn_);
        txn.exec_params("DELETE FROM tags WHERE id = $1", tag_id);
        txn.commit();
        return true;
    } catch (const std::exception& e) {
        last_error_ = std::string("Delete tag error: ") + e.what();
        return false;
    }
}
bool DatabaseManager::setGameTags(int game_id, const std::vector<int>& tag_ids) {
    try {
        pqxx::work txn(*conn_);
        txn.exec_params("DELETE FROM game_tags WHERE game_id = $1", game_id);
        for (int tag_id : tag_ids) {
            txn.exec_params(
                "INSERT INTO game_tags (game_id, tag_id) VALUES ($1, $2) ON CONFLICT DO NOTHING",
                game_id, tag_id
            );
        }
        txn.commit();
        return true;
    } catch (const std::exception& e) {
        last_error_ = std::string("Set game tags error: ") + e.what();
        return false;
    }
}
std::vector<int> DatabaseManager::getGameTagIds(int game_id) {
    std::vector<int> tag_ids;
    try {
        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec_params(
            "SELECT tag_id FROM game_tags WHERE game_id = $1",
            game_id
        );
        for (const auto& row : r) {
            tag_ids.push_back(row[0].as<int>());
        }
        txn.commit();
    } catch (const std::exception& e) {
        last_error_ = std::string("Get game tag ids error: ") + e.what();
    }
    return tag_ids;
}
std::vector<Tag> DatabaseManager::getGameTags(int game_id) {
    std::vector<Tag> tags;
    try {
        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec_params(
            "SELECT t.id, t.name, t.user_id, t.color "
            "FROM tags t "
            "INNER JOIN game_tags gt ON t.id = gt.tag_id "
            "WHERE gt.game_id = $1 "
            "ORDER BY t.name",
            game_id
        );
        for (const auto& row : r) {
            Tag tag;
            tag.id = row["id"].as<int>();
            tag.name = row["name"].as<std::string>();
            tag.user_id = row["user_id"].as<int>();
            tag.color = row["color"].is_null() ? "#808080" : row["color"].as<std::string>();
            tags.push_back(tag);
        }
        txn.commit();
    } catch (const std::exception& e) {
        last_error_ = std::string("Get game tags error: ") + e.what();
    }
    return tags;
}
std::string DatabaseManager::aggregateGameTags(int game_id) {
    try {
        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec_params(
            "SELECT STRING_AGG(t.name, ', ' ORDER BY t.name) as tags "
            "FROM tags t "
            "INNER JOIN game_tags gt ON t.id = gt.tag_id "
            "WHERE gt.game_id = $1",
            game_id
        );
        txn.commit();
        if (!r.empty() && !r[0][0].is_null()) {
            return r[0][0].as<std::string>();
        }
    } catch (const std::exception& e) {
    }
    return "";
}
bool DatabaseManager::addGame(const Game& game) {
    try {
        pqxx::work txn(*conn_);
        int genre_id = game.genre_id;
        if (genre_id == 0 && !game.genre.empty()) {
            pqxx::result gr = txn.exec_params(
                "SELECT id FROM genres WHERE name = $1",
                game.genre
            );
            if (!gr.empty()) {
                genre_id = gr[0][0].as<int>();
            }
        }
        pqxx::result r = txn.exec_params(
            "INSERT INTO games (name, disk_space, ram_usage, vram_required, genre_id, "
            "completed, url, user_id, rating, is_favorite, is_installed, notes) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12) RETURNING id",
            game.name, game.disk_space, game.ram_usage, game.vram_required, genre_id,
            game.completed, game.url, game.user_id, game.rating, game.is_favorite,
            game.is_installed, game.notes
        );
        int new_game_id = r[0][0].as<int>();
        txn.commit();
        if (!game.tag_ids.empty()) {
            setGameTags(new_game_id, game.tag_ids);
        } else if (!game.tags.empty()) {
            std::vector<int> tag_ids;
            std::stringstream ss(game.tags);
            std::string tag_name;
            while (std::getline(ss, tag_name, ',')) {
                size_t start = tag_name.find_first_not_of(" \t");
                size_t end = tag_name.find_last_not_of(" \t");
                if (start != std::string::npos && end != std::string::npos) {
                    tag_name = tag_name.substr(start, end - start + 1);
                    Tag existing = getTagByName(tag_name, game.user_id);
                    if (existing.id > 0) {
                        tag_ids.push_back(existing.id);
                    } else {
                        int new_tag_id = addTag(tag_name, game.user_id);
                        if (new_tag_id > 0) {
                            tag_ids.push_back(new_tag_id);
                        }
                    }
                }
            }
            if (!tag_ids.empty()) {
                setGameTags(new_game_id, tag_ids);
            }
        }
        return true;
    } catch (const std::exception& e) {
        last_error_ = std::string("Add game error: ") + e.what();
        return false;
    }
}
bool DatabaseManager::updateGame(const Game& game) {
    try {
        pqxx::work txn(*conn_);
        int genre_id = game.genre_id;
        if (genre_id == 0 && !game.genre.empty()) {
            pqxx::result gr = txn.exec_params(
                "SELECT id FROM genres WHERE name = $1",
                game.genre
            );
            if (!gr.empty()) {
                genre_id = gr[0][0].as<int>();
            }
        }
        txn.exec_params(
            "UPDATE games SET name = $1, disk_space = $2, ram_usage = $3, "
            "vram_required = $4, genre_id = $5, completed = $6, url = $7, "
            "rating = $8, is_favorite = $9, is_installed = $10, notes = $11 "
            "WHERE id = $12 AND user_id = $13",
            game.name, game.disk_space, game.ram_usage, game.vram_required, genre_id,
            game.completed, game.url, game.rating, game.is_favorite, game.is_installed,
            game.notes, game.id, game.user_id
        );
        txn.commit();
        if (!game.tag_ids.empty()) {
            setGameTags(game.id, game.tag_ids);
        } else if (!game.tags.empty()) {
            std::vector<int> tag_ids;
            std::stringstream ss(game.tags);
            std::string tag_name;
            while (std::getline(ss, tag_name, ',')) {
                size_t start = tag_name.find_first_not_of(" \t");
                size_t end = tag_name.find_last_not_of(" \t");
                if (start != std::string::npos && end != std::string::npos) {
                    tag_name = tag_name.substr(start, end - start + 1);
                    Tag existing = getTagByName(tag_name, game.user_id);
                    if (existing.id > 0) {
                        tag_ids.push_back(existing.id);
                    } else {
                        int new_tag_id = addTag(tag_name, game.user_id);
                        if (new_tag_id > 0) {
                            tag_ids.push_back(new_tag_id);
                        }
                    }
                }
            }
            setGameTags(game.id, tag_ids);
        } else {
            setGameTags(game.id, {});
        }
        return true;
    } catch (const std::exception& e) {
        last_error_ = std::string("Update game error: ") + e.what();
        return false;
    }
}
bool DatabaseManager::deleteGame(int game_id, int user_id) {
    try {
        pqxx::work txn(*conn_);
        txn.exec_params(
            "DELETE FROM games WHERE id = $1 AND user_id = $2",
            game_id, user_id
        );
        txn.commit();
        return true;
    } catch (const std::exception& e) {
        last_error_ = std::string("Delete game error: ") + e.what();
        return false;
    }
}
bool DatabaseManager::deleteGameByName(const std::string& name, int user_id) {
    try {
        pqxx::work txn(*conn_);
        txn.exec_params(
            "DELETE FROM games WHERE name = $1 AND user_id = $2",
            name, user_id
        );
        txn.commit();
        return true;
    } catch (const std::exception& e) {
        last_error_ = std::string("Delete game by name error: ") + e.what();
        return false;
    }
}
std::vector<Game> DatabaseManager::getAllGames(int user_id) {
    std::vector<Game> games;
    try {
        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec_params(
            "SELECT g.id, g.name, g.disk_space, g.ram_usage, g.vram_required, "
            "g.genre_id, COALESCE(gen.name, 'Unknown') as genre, g.completed, g.url, "
            "g.user_id, g.rating, g.is_favorite, g.is_installed, g.notes "
            "FROM games g "
            "LEFT JOIN genres gen ON g.genre_id = gen.id "
            "WHERE g.user_id = $1 "
            "ORDER BY g.name",
            user_id
        );
        for (const auto& row : r) {
            Game game;
            game.id = row["id"].as<int>();
            game.name = row["name"].as<std::string>();
            game.disk_space = row["disk_space"].as<double>();
            game.ram_usage = row["ram_usage"].as<double>();
            game.vram_required = row["vram_required"].as<double>();
            game.genre_id = row["genre_id"].is_null() ? 0 : row["genre_id"].as<int>();
            game.genre = row["genre"].as<std::string>();
            game.completed = row["completed"].as<bool>();
            game.url = row["url"].is_null() ? "" : row["url"].as<std::string>();
            game.user_id = row["user_id"].as<int>();
            game.rating = row["rating"].is_null() ? -1 : row["rating"].as<int>();
            game.is_favorite = row["is_favorite"].is_null() ? false : row["is_favorite"].as<bool>();
            game.is_installed = row["is_installed"].is_null() ? false : row["is_installed"].as<bool>();
            game.notes = row["notes"].is_null() ? "" : row["notes"].as<std::string>();
            game.tags = aggregateGameTags(game.id);
            games.push_back(game);
        }
        txn.commit();
    } catch (const std::exception& e) {
        last_error_ = std::string("Get all games error: ") + e.what();
    }
    return games;
}
std::string DatabaseManager::buildFilterCondition(const GameFilter& filter, int user_id) {
    std::stringstream ss;
    ss << "g.user_id = " << user_id;
    if (filter.filter_completed) {
        ss << " AND g.completed = " << (filter.completed_value ? "TRUE" : "FALSE");
    }
    if (filter.filter_genre && filter.genre_id > 0) {
        ss << " AND g.genre_id = " << filter.genre_id;
    }
    if (filter.filter_disk_space_min) {
        ss << " AND g.disk_space >= " << filter.disk_space_min;
    }
    if (filter.filter_disk_space_max) {
        ss << " AND g.disk_space <= " << filter.disk_space_max;
    }
    if (filter.filter_ram_min) {
        ss << " AND g.ram_usage >= " << filter.ram_min;
    }
    if (filter.filter_ram_max) {
        ss << " AND g.ram_usage <= " << filter.ram_max;
    }
    if (filter.filter_vram_min) {
        ss << " AND g.vram_required >= " << filter.vram_min;
    }
    if (filter.filter_vram_max) {
        ss << " AND g.vram_required <= " << filter.vram_max;
    }
    if (filter.filter_favorite) {
        ss << " AND g.is_favorite = " << (filter.favorite_value ? "TRUE" : "FALSE");
    }
    if (filter.filter_installed) {
        ss << " AND g.is_installed = " << (filter.installed_value ? "TRUE" : "FALSE");
    }
    if (filter.filter_has_rating) {
        if (filter.has_rating_value) {
            ss << " AND g.rating >= 0";
        } else {
            ss << " AND g.rating = -1";
        }
    }
    if (filter.filter_tag && filter.tag_id > 0) {
        ss << " AND EXISTS (SELECT 1 FROM game_tags gt WHERE gt.game_id = g.id AND gt.tag_id = " << filter.tag_id << ")";
    }
    return ss.str();
}
std::vector<Game> DatabaseManager::getFilteredGames(int user_id, const GameFilter& filter) {
    std::vector<Game> games;
    try {
        pqxx::work txn(*conn_);
        std::string condition = buildFilterCondition(filter, user_id);
        std::string query = 
            "SELECT g.id, g.name, g.disk_space, g.ram_usage, g.vram_required, "
            "g.genre_id, COALESCE(gen.name, 'Unknown') as genre, g.completed, g.url, "
            "g.user_id, g.rating, g.is_favorite, g.is_installed, g.notes "
            "FROM games g "
            "LEFT JOIN genres gen ON g.genre_id = gen.id "
            "WHERE " + condition + " "
            "ORDER BY g.name";
        pqxx::result r = txn.exec(query);
        for (const auto& row : r) {
            Game game;
            game.id = row["id"].as<int>();
            game.name = row["name"].as<std::string>();
            game.disk_space = row["disk_space"].as<double>();
            game.ram_usage = row["ram_usage"].as<double>();
            game.vram_required = row["vram_required"].as<double>();
            game.genre_id = row["genre_id"].is_null() ? 0 : row["genre_id"].as<int>();
            game.genre = row["genre"].as<std::string>();
            game.completed = row["completed"].as<bool>();
            game.url = row["url"].is_null() ? "" : row["url"].as<std::string>();
            game.user_id = row["user_id"].as<int>();
            game.rating = row["rating"].is_null() ? -1 : row["rating"].as<int>();
            game.is_favorite = row["is_favorite"].is_null() ? false : row["is_favorite"].as<bool>();
            game.is_installed = row["is_installed"].is_null() ? false : row["is_installed"].as<bool>();
            game.notes = row["notes"].is_null() ? "" : row["notes"].as<std::string>();
            game.tags = aggregateGameTags(game.id);
            games.push_back(game);
        }
        txn.commit();
    } catch (const std::exception& e) {
        last_error_ = std::string("Get filtered games error: ") + e.what();
    }
    return games;
}
Game DatabaseManager::getGameById(int game_id, int user_id) {
    Game game;
    try {
        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec_params(
            "SELECT g.id, g.name, g.disk_space, g.ram_usage, g.vram_required, "
            "g.genre_id, COALESCE(gen.name, 'Unknown') as genre, g.completed, g.url, "
            "g.user_id, g.rating, g.is_favorite, g.is_installed, g.notes "
            "FROM games g "
            "LEFT JOIN genres gen ON g.genre_id = gen.id "
            "WHERE g.id = $1 AND g.user_id = $2",
            game_id, user_id
        );
        if (!r.empty()) {
            game.id = r[0]["id"].as<int>();
            game.name = r[0]["name"].as<std::string>();
            game.disk_space = r[0]["disk_space"].as<double>();
            game.ram_usage = r[0]["ram_usage"].as<double>();
            game.vram_required = r[0]["vram_required"].as<double>();
            game.genre_id = r[0]["genre_id"].is_null() ? 0 : r[0]["genre_id"].as<int>();
            game.genre = r[0]["genre"].as<std::string>();
            game.completed = r[0]["completed"].as<bool>();
            game.url = r[0]["url"].is_null() ? "" : r[0]["url"].as<std::string>();
            game.user_id = r[0]["user_id"].as<int>();
            game.rating = r[0]["rating"].is_null() ? -1 : r[0]["rating"].as<int>();
            game.is_favorite = r[0]["is_favorite"].is_null() ? false : r[0]["is_favorite"].as<bool>();
            game.is_installed = r[0]["is_installed"].is_null() ? false : r[0]["is_installed"].as<bool>();
            game.notes = r[0]["notes"].is_null() ? "" : r[0]["notes"].as<std::string>();
            game.tags = aggregateGameTags(game.id);
        }
        txn.commit();
    } catch (const std::exception& e) {
        last_error_ = std::string("Get game by id error: ") + e.what();
    }
    return game;
}
Game DatabaseManager::getGameByName(const std::string& name, int user_id) {
    Game game;
    try {
        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec_params(
            "SELECT g.id, g.name, g.disk_space, g.ram_usage, g.vram_required, "
            "g.genre_id, COALESCE(gen.name, 'Unknown') as genre, g.completed, g.url, "
            "g.user_id, g.rating, g.is_favorite, g.is_installed, g.notes "
            "FROM games g "
            "LEFT JOIN genres gen ON g.genre_id = gen.id "
            "WHERE g.name = $1 AND g.user_id = $2",
            name, user_id
        );
        if (!r.empty()) {
            game.id = r[0]["id"].as<int>();
            game.name = r[0]["name"].as<std::string>();
            game.disk_space = r[0]["disk_space"].as<double>();
            game.ram_usage = r[0]["ram_usage"].as<double>();
            game.vram_required = r[0]["vram_required"].as<double>();
            game.genre_id = r[0]["genre_id"].is_null() ? 0 : r[0]["genre_id"].as<int>();
            game.genre = r[0]["genre"].as<std::string>();
            game.completed = r[0]["completed"].as<bool>();
            game.url = r[0]["url"].is_null() ? "" : r[0]["url"].as<std::string>();
            game.user_id = r[0]["user_id"].as<int>();
            game.rating = r[0]["rating"].is_null() ? -1 : r[0]["rating"].as<int>();
            game.is_favorite = r[0]["is_favorite"].is_null() ? false : r[0]["is_favorite"].as<bool>();
            game.is_installed = r[0]["is_installed"].is_null() ? false : r[0]["is_installed"].as<bool>();
            game.notes = r[0]["notes"].is_null() ? "" : r[0]["notes"].as<std::string>();
            game.tags = aggregateGameTags(game.id);
        }
        txn.commit();
    } catch (const std::exception& e) {
        last_error_ = std::string("Get game by name error: ") + e.what();
    }
    return game;
}
bool DatabaseManager::updateGameNotes(int game_id, int user_id, const std::string& notes) {
    try {
        pqxx::work txn(*conn_);
        txn.exec_params(
            "UPDATE games SET notes = $1 WHERE id = $2 AND user_id = $3",
            notes, game_id, user_id
        );
        txn.commit();
        return true;
    } catch (const std::exception& e) {
        last_error_ = std::string("Update notes error: ") + e.what();
        return false;
    }
}
GameStats DatabaseManager::getGameStats(int user_id) {
    GameStats stats;
    try {
        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec_params(
            "SELECT COUNT(*) FROM games WHERE user_id = $1",
            user_id
        );
        stats.total_games = r[0][0].as<int>();
        r = txn.exec_params(
            "SELECT COUNT(*) FROM games WHERE user_id = $1 AND is_favorite = TRUE",
            user_id
        );
        stats.favorites_count = r[0][0].as<int>();
        r = txn.exec_params(
            "SELECT COUNT(*) FROM games WHERE user_id = $1 AND completed = TRUE",
            user_id
        );
        stats.completed_count = r[0][0].as<int>();
        r = txn.exec_params(
            "SELECT COUNT(*) FROM games WHERE user_id = $1 AND rating = -1",
            user_id
        );
        stats.no_rating_count = r[0][0].as<int>();
        r = txn.exec_params(
            "SELECT COUNT(*), COALESCE(SUM(disk_space), 0) FROM games WHERE user_id = $1 AND is_installed = TRUE",
            user_id
        );
        stats.installed_count = r[0][0].as<int>();
        stats.installed_disk_space = r[0][1].as<double>();
        r = txn.exec_params(
            "SELECT COUNT(*) FROM games WHERE user_id = $1 AND (url IS NULL OR url = '')",
            user_id
        );
        stats.no_url_count = r[0][0].as<int>();
        txn.commit();
    } catch (const std::exception& e) {
        last_error_ = std::string("Get stats error: ") + e.what();
    }
    return stats;
}
std::vector<GenreStats> DatabaseManager::getGenreStatistics(int user_id) {
    std::vector<GenreStats> stats;
    try {
        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec_params(
            "SELECT gen.id, gen.name, "
            "COUNT(g.id) as games_count, "
            "COUNT(CASE WHEN g.completed THEN 1 END) as completed_count, "
            "AVG(CASE WHEN g.rating >= 0 THEN g.rating END) as avg_rating, "
            "SUM(g.disk_space) as total_disk_space "
            "FROM genres gen "
            "LEFT JOIN games g ON gen.id = g.genre_id AND g.user_id = $1 "
            "GROUP BY gen.id, gen.name "
            "HAVING COUNT(g.id) > 0 "
            "ORDER BY games_count DESC",
            user_id
        );
        for (const auto& row : r) {
            GenreStats gs;
            gs.genre_id = row["id"].as<int>();
            gs.genre_name = row["name"].as<std::string>();
            gs.games_count = row["games_count"].as<int>();
            gs.completed_count = row["completed_count"].as<int>();
            gs.avg_rating = row["avg_rating"].is_null() ? 0.0 : row["avg_rating"].as<double>();
            gs.total_disk_space = row["total_disk_space"].is_null() ? 0.0 : row["total_disk_space"].as<double>();
            stats.push_back(gs);
        }
        txn.commit();
    } catch (const std::exception& e) {
        last_error_ = std::string("Get genre stats error: ") + e.what();
    }
    return stats;
}
std::vector<Game> DatabaseManager::getTopRatedGames(int user_id, int limit) {
    std::vector<Game> games;
    try {
        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec_params(
            "SELECT g.id, g.name, g.disk_space, g.ram_usage, g.vram_required, "
            "g.genre_id, COALESCE(gen.name, 'Unknown') as genre, g.completed, g.url, "
            "g.user_id, g.rating, g.is_favorite, g.is_installed, g.notes "
            "FROM games g "
            "LEFT JOIN genres gen ON g.genre_id = gen.id "
            "WHERE g.user_id = $1 AND g.rating >= 0 "
            "ORDER BY g.rating DESC, g.name ASC "
            "LIMIT $2",
            user_id, limit
        );
        for (const auto& row : r) {
            Game game;
            game.id = row["id"].as<int>();
            game.name = row["name"].as<std::string>();
            game.disk_space = row["disk_space"].as<double>();
            game.ram_usage = row["ram_usage"].as<double>();
            game.vram_required = row["vram_required"].as<double>();
            game.genre_id = row["genre_id"].is_null() ? 0 : row["genre_id"].as<int>();
            game.genre = row["genre"].as<std::string>();
            game.completed = row["completed"].as<bool>();
            game.url = row["url"].is_null() ? "" : row["url"].as<std::string>();
            game.user_id = row["user_id"].as<int>();
            game.rating = row["rating"].as<int>();
            game.is_favorite = row["is_favorite"].is_null() ? false : row["is_favorite"].as<bool>();
            game.is_installed = row["is_installed"].is_null() ? false : row["is_installed"].as<bool>();
            game.notes = row["notes"].is_null() ? "" : row["notes"].as<std::string>();
            games.push_back(game);
        }
        txn.commit();
    } catch (const std::exception& e) {
        last_error_ = std::string("Get top rated games error: ") + e.what();
    }
    return games;
}
std::vector<Game> DatabaseManager::getGamesWithTags(int user_id) {
    std::vector<Game> games;
    try {
        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec_params(
            "SELECT g.id, g.name, g.disk_space, g.ram_usage, g.vram_required, "
            "g.genre_id, COALESCE(gen.name, 'Unknown') as genre, g.completed, g.url, "
            "g.user_id, g.rating, g.is_favorite, g.is_installed, g.notes, "
            "STRING_AGG(t.name, ', ' ORDER BY t.name) as tags "
            "FROM games g "
            "LEFT JOIN genres gen ON g.genre_id = gen.id "
            "LEFT JOIN game_tags gt ON g.id = gt.game_id "
            "LEFT JOIN tags t ON gt.tag_id = t.id "
            "WHERE g.user_id = $1 "
            "GROUP BY g.id, g.name, g.disk_space, g.ram_usage, g.vram_required, "
            "g.genre_id, gen.name, g.completed, g.url, g.user_id, g.rating, "
            "g.is_favorite, g.is_installed, g.notes "
            "HAVING COUNT(t.id) > 0 "
            "ORDER BY g.name",
            user_id
        );
        for (const auto& row : r) {
            Game game;
            game.id = row["id"].as<int>();
            game.name = row["name"].as<std::string>();
            game.disk_space = row["disk_space"].as<double>();
            game.ram_usage = row["ram_usage"].as<double>();
            game.vram_required = row["vram_required"].as<double>();
            game.genre_id = row["genre_id"].is_null() ? 0 : row["genre_id"].as<int>();
            game.genre = row["genre"].as<std::string>();
            game.completed = row["completed"].as<bool>();
            game.url = row["url"].is_null() ? "" : row["url"].as<std::string>();
            game.user_id = row["user_id"].as<int>();
            game.rating = row["rating"].is_null() ? -1 : row["rating"].as<int>();
            game.is_favorite = row["is_favorite"].is_null() ? false : row["is_favorite"].as<bool>();
            game.is_installed = row["is_installed"].is_null() ? false : row["is_installed"].as<bool>();
            game.notes = row["notes"].is_null() ? "" : row["notes"].as<std::string>();
            game.tags = row["tags"].is_null() ? "" : row["tags"].as<std::string>();
            games.push_back(game);
        }
        txn.commit();
    } catch (const std::exception& e) {
        last_error_ = std::string("Get games with tags error: ") + e.what();
    }
    return games;
}
double DatabaseManager::getAverageRatingByGenre(int genre_id, int user_id) {
    try {
        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec_params(
            "SELECT AVG(rating) FROM games "
            "WHERE genre_id = $1 AND user_id = $2 AND rating >= 0",
            genre_id, user_id
        );
        txn.commit();
        if (!r.empty() && !r[0][0].is_null()) {
            return r[0][0].as<double>();
        }
    } catch (const std::exception& e) {
        last_error_ = std::string("Get avg rating error: ") + e.what();
    }
    return 0.0;
}
int DatabaseManager::countGamesAboveRating(int user_id, int min_rating) {
    try {
        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec_params(
            "SELECT genre_id, COUNT(*) as cnt "
            "FROM games "
            "WHERE user_id = $1 AND rating >= $2 "
            "GROUP BY genre_id "
            "HAVING COUNT(*) >= 1",
            user_id, min_rating
        );
        txn.commit();
        int total = 0;
        for (const auto& row : r) {
            total += row["cnt"].as<int>();
        }
        return total;
    } catch (const std::exception& e) {
        last_error_ = std::string("Count games above rating error: ") + e.what();
        return 0;
    }
}
std::vector<Game> DatabaseManager::searchGames(int user_id, const std::string& search_term) {
    std::vector<Game> games;
    try {
        pqxx::work txn(*conn_);
        std::string pattern = "%" + search_term + "%";
        pqxx::result r = txn.exec_params(
            "SELECT g.id, g.name, g.disk_space, g.ram_usage, g.vram_required, "
            "g.genre_id, COALESCE(gen.name, 'Unknown') as genre, g.completed, g.url, "
            "g.user_id, g.rating, g.is_favorite, g.is_installed, g.notes "
            "FROM games g "
            "LEFT JOIN genres gen ON g.genre_id = gen.id "
            "WHERE g.user_id = $1 AND g.name ILIKE $2 "
            "ORDER BY g.name",
            user_id, pattern
        );
        for (const auto& row : r) {
            Game game;
            game.id = row["id"].as<int>();
            game.name = row["name"].as<std::string>();
            game.disk_space = row["disk_space"].as<double>();
            game.ram_usage = row["ram_usage"].as<double>();
            game.vram_required = row["vram_required"].as<double>();
            game.genre_id = row["genre_id"].is_null() ? 0 : row["genre_id"].as<int>();
            game.genre = row["genre"].as<std::string>();
            game.completed = row["completed"].as<bool>();
            game.url = row["url"].is_null() ? "" : row["url"].as<std::string>();
            game.user_id = row["user_id"].as<int>();
            game.rating = row["rating"].is_null() ? -1 : row["rating"].as<int>();
            game.is_favorite = row["is_favorite"].is_null() ? false : row["is_favorite"].as<bool>();
            game.is_installed = row["is_installed"].is_null() ? false : row["is_installed"].as<bool>();
            game.notes = row["notes"].is_null() ? "" : row["notes"].as<std::string>();
            games.push_back(game);
        }
        txn.commit();
    } catch (const std::exception& e) {
        last_error_ = std::string("Search games error: ") + e.what();
    }
    return games;
}
std::vector<std::pair<std::string, int>> DatabaseManager::getTagUsageStats(int user_id) {
    std::vector<std::pair<std::string, int>> stats;
    try {
        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec_params(
            "SELECT t.name, COUNT(gt.game_id) as usage_count "
            "FROM tags t "
            "LEFT JOIN game_tags gt ON t.id = gt.tag_id "
            "WHERE t.user_id = $1 "
            "GROUP BY t.id, t.name "
            "ORDER BY usage_count DESC, t.name",
            user_id
        );
        for (const auto& row : r) {
            stats.emplace_back(row["name"].as<std::string>(), row["usage_count"].as<int>());
        }
        txn.commit();
    } catch (const std::exception& e) {
        last_error_ = std::string("Get tag usage stats error: ") + e.what();
    }
    return stats;
}
std::vector<Game> DatabaseManager::getGamesCompletedByGenre(int user_id, int genre_id) {
    std::vector<Game> games;
    try {
        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec_params(
            "SELECT g.id, g.name, g.disk_space, g.ram_usage, g.vram_required, "
            "g.genre_id, gen.name as genre, g.completed, g.url, "
            "g.user_id, g.rating, g.is_favorite, g.is_installed, g.notes "
            "FROM games g "
            "INNER JOIN genres gen ON g.genre_id = gen.id "
            "WHERE g.user_id = $1 AND g.genre_id = $2 AND g.completed = TRUE "
            "ORDER BY g.rating DESC NULLS LAST, g.name",
            user_id, genre_id
        );
        for (const auto& row : r) {
            Game game;
            game.id = row["id"].as<int>();
            game.name = row["name"].as<std::string>();
            game.disk_space = row["disk_space"].as<double>();
            game.ram_usage = row["ram_usage"].as<double>();
            game.vram_required = row["vram_required"].as<double>();
            game.genre_id = row["genre_id"].as<int>();
            game.genre = row["genre"].as<std::string>();
            game.completed = row["completed"].as<bool>();
            game.url = row["url"].is_null() ? "" : row["url"].as<std::string>();
            game.user_id = row["user_id"].as<int>();
            game.rating = row["rating"].is_null() ? -1 : row["rating"].as<int>();
            game.is_favorite = row["is_favorite"].is_null() ? false : row["is_favorite"].as<bool>();
            game.is_installed = row["is_installed"].is_null() ? false : row["is_installed"].as<bool>();
            game.notes = row["notes"].is_null() ? "" : row["notes"].as<std::string>();
            games.push_back(game);
        }
        txn.commit();
    } catch (const std::exception& e) {
        last_error_ = std::string("Get completed games by genre error: ") + e.what();
    }
    return games;
}
std::vector<Game> DatabaseManager::getUnplayedHighRatedGames(int user_id) {
    std::vector<Game> games;
    try {
        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec_params(
            "SELECT g.id, g.name, g.disk_space, g.ram_usage, g.vram_required, "
            "g.genre_id, gen.name as genre, g.completed, g.url, "
            "g.user_id, g.rating, g.is_favorite, g.is_installed, g.notes "
            "FROM games g "
            "INNER JOIN genres gen ON g.genre_id = gen.id "
            "WHERE g.user_id = $1 AND g.completed = FALSE "
            "AND g.genre_id IN ("
            "    SELECT genre_id FROM games "
            "    WHERE user_id = $1 AND rating >= 0 "
            "    GROUP BY genre_id "
            "    HAVING AVG(rating) >= 7"
            ") "
            "ORDER BY g.name",
            user_id
        );
        for (const auto& row : r) {
            Game game;
            game.id = row["id"].as<int>();
            game.name = row["name"].as<std::string>();
            game.disk_space = row["disk_space"].as<double>();
            game.ram_usage = row["ram_usage"].as<double>();
            game.vram_required = row["vram_required"].as<double>();
            game.genre_id = row["genre_id"].as<int>();
            game.genre = row["genre"].as<std::string>();
            game.completed = row["completed"].as<bool>();
            game.url = row["url"].is_null() ? "" : row["url"].as<std::string>();
            game.user_id = row["user_id"].as<int>();
            game.rating = row["rating"].is_null() ? -1 : row["rating"].as<int>();
            game.is_favorite = row["is_favorite"].is_null() ? false : row["is_favorite"].as<bool>();
            game.is_installed = row["is_installed"].is_null() ? false : row["is_installed"].as<bool>();
            game.notes = row["notes"].is_null() ? "" : row["notes"].as<std::string>();
            games.push_back(game);
        }
        txn.commit();
    } catch (const std::exception& e) {
        last_error_ = std::string("Get unplayed high rated games error: ") + e.what();
    }
    return games;
}
bool DatabaseManager::writeGamesToFile(const std::string& filename, const std::vector<Game>& games) {
    try {
        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            last_error_ = "Cannot open file for writing: " + filename;
            return false;
        }
        BinaryFileHeader header;
        header.record_count = static_cast<uint32_t>(games.size());
        std::vector<BinaryGameRecord> records;
        for (const auto& game : games) {
            BinaryGameRecord record;
            record.id = game.id;
            std::strncpy(record.name, game.name.c_str(), sizeof(record.name) - 1);
            record.disk_space = game.disk_space;
            record.ram_usage = game.ram_usage;
            record.vram_required = game.vram_required;
            record.genre_id = game.genre_id;
            std::strncpy(record.genre, game.genre.c_str(), sizeof(record.genre) - 1);
            record.completed = game.completed ? 1 : 0;
            std::strncpy(record.url, game.url.c_str(), sizeof(record.url) - 1);
            record.user_id = game.user_id;
            record.rating = game.rating;
            record.is_favorite = game.is_favorite ? 1 : 0;
            record.is_installed = game.is_installed ? 1 : 0;
            std::strncpy(record.notes, game.notes.c_str(), sizeof(record.notes) - 1);
            std::strncpy(record.tags, game.tags.c_str(), sizeof(record.tags) - 1);
            records.push_back(record);
        }
        std::string data_to_hash;
        for (const auto& record : records) {
            data_to_hash.append(reinterpret_cast<const char*>(&record), sizeof(record));
        }
        std::string hash = HashUtils::sha256(data_to_hash);
        std::strncpy(header.hash, hash.c_str(), sizeof(header.hash) - 1);
        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        for (const auto& record : records) {
            file.write(reinterpret_cast<const char*>(&record), sizeof(record));
        }
        file.close();
        return true;
    } catch (const std::exception& e) {
        last_error_ = std::string("Write file error: ") + e.what();
        return false;
    }
}
bool DatabaseManager::exportToBinaryFile(const std::string& filename, int user_id) {
    std::vector<Game> games = getAllGames(user_id);
    return writeGamesToFile(filename, games);
}
bool DatabaseManager::exportFilteredToBinaryFile(const std::string& filename, int user_id, 
                                                   const GameFilter& filter) {
    std::vector<Game> games = getFilteredGames(user_id, filter);
    return writeGamesToFile(filename, games);
}
FileVerificationResult DatabaseManager::verifyBinaryFile(const std::string& filename) {
    try {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            return FileVerificationResult::FILE_NOT_FOUND;
        }
        BinaryFileHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (header.magic != FILE_MAGIC) {
            return FileVerificationResult::INVALID_MAGIC;
        }
        if (header.version > FILE_VERSION) {
            return FileVerificationResult::INVALID_VERSION;
        }
        std::string data_to_hash;
        for (uint32_t i = 0; i < header.record_count; ++i) {
            BinaryGameRecord record;
            file.read(reinterpret_cast<char*>(&record), sizeof(record));
            data_to_hash.append(reinterpret_cast<const char*>(&record), sizeof(record));
        }
        file.close();
        std::string computed_hash = HashUtils::sha256(data_to_hash);
        std::string stored_hash(header.hash, strnlen(header.hash, sizeof(header.hash)));
        if (computed_hash != stored_hash) {
            return FileVerificationResult::HASH_MISMATCH;
        }
        return FileVerificationResult::OK;
    } catch (const std::exception& e) {
        last_error_ = std::string("Verification error: ") + e.what();
        return FileVerificationResult::READ_ERROR;
    }
}
std::string DatabaseManager::getVerificationErrorText(FileVerificationResult result) {
    switch (result) {
        case FileVerificationResult::OK:
            return "Файл корректен";
        case FileVerificationResult::FILE_NOT_FOUND:
            return "Файл не найден";
        case FileVerificationResult::INVALID_MAGIC:
            return "Неверный формат файла (не является файлом Temporium)";
        case FileVerificationResult::INVALID_VERSION:
            return "Неподдерживаемая версия формата файла";
        case FileVerificationResult::HASH_MISMATCH:
            return "Файл поврежден или модифицирован (контрольная сумма не совпадает)";
        case FileVerificationResult::READ_ERROR:
            return "Ошибка чтения файла";
        default:
            return "Неизвестная ошибка";
    }
}
bool DatabaseManager::importFromBinaryFile(const std::string& filename, int user_id) {
    FileVerificationResult verification = verifyBinaryFile(filename);
    if (verification != FileVerificationResult::OK) {
        last_error_ = getVerificationErrorText(verification);
        return false;
    }
    try {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            last_error_ = "Cannot open file for reading: " + filename;
            return false;
        }
        BinaryFileHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        for (uint32_t i = 0; i < header.record_count; ++i) {
            BinaryGameRecord record;
            file.read(reinterpret_cast<char*>(&record), sizeof(record));
            Game game;
            game.name = record.name;
            game.disk_space = record.disk_space;
            game.ram_usage = record.ram_usage;
            game.vram_required = record.vram_required;
            game.genre = record.genre;
            game.completed = record.completed != 0;
            game.url = record.url;
            game.user_id = user_id;
            game.rating = record.rating;
            game.is_favorite = record.is_favorite != 0;
            game.is_installed = record.is_installed != 0;
            game.notes = record.notes;
            game.tags = record.tags;
            addGame(game);
        }
        file.close();
        return true;
    } catch (const std::exception& e) {
        last_error_ = std::string("Import error: ") + e.what();
        return false;
    }
}
std::vector<Game> DatabaseManager::readBinaryFile(const std::string& filename) {
    std::vector<Game> games;
    try {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            last_error_ = "Cannot open file for reading: " + filename;
            return games;
        }
        BinaryFileHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (header.magic != FILE_MAGIC) {
            last_error_ = "Invalid file format";
            file.close();
            return games;
        }
        for (uint32_t i = 0; i < header.record_count; ++i) {
            BinaryGameRecord record;
            file.read(reinterpret_cast<char*>(&record), sizeof(record));
            Game game;
            game.id = record.id;
            game.name = record.name;
            game.disk_space = record.disk_space;
            game.ram_usage = record.ram_usage;
            game.vram_required = record.vram_required;
            game.genre_id = record.genre_id;
            game.genre = record.genre;
            game.completed = record.completed != 0;
            game.url = record.url;
            game.user_id = record.user_id;
            game.rating = record.rating;
            game.is_favorite = record.is_favorite != 0;
            game.is_installed = record.is_installed != 0;
            game.notes = record.notes;
            game.tags = record.tags;
            games.push_back(game);
        }
        file.close();
    } catch (const std::exception& e) {
        last_error_ = std::string("Read binary file error: ") + e.what();
    }
    return games;
}
std::string DatabaseManager::getLastError() const {
    return last_error_;
}
} // namespace Temporium