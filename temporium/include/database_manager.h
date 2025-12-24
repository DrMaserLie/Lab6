#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <string>
#include <vector>
#include <memory>
#include <pqxx/pqxx>
#include "types.h"

namespace Temporium {

// Результат проверки файла при импорте
enum class FileVerificationResult {
    OK,
    FILE_NOT_FOUND,
    INVALID_MAGIC,
    INVALID_VERSION,
    HASH_MISMATCH,
    READ_ERROR
};

// Статистика игр для отображения в статусбаре
struct GameStats {
    int total_games = 0;
    int favorites_count = 0;
    int completed_count = 0;
    int no_rating_count = 0;
    int installed_count = 0;
    double installed_disk_space = 0.0;  // ГБ
    int no_url_count = 0;
};

// Статистика по жанрам
struct GenreStats {
    int genre_id;
    std::string genre_name;
    int games_count;
    int completed_count;
    double avg_rating;
    double total_disk_space;
};

class DatabaseManager {
public:
    DatabaseManager();
    ~DatabaseManager();
    
    // Подключение к базе данных
    bool connect(const std::string& host, int port, 
                 const std::string& dbname, 
                 const std::string& user, 
                 const std::string& password);
    
    void disconnect();
    bool isConnected() const;
    
    // Инициализация таблиц
    bool initializeTables();
    
    // ============================================================
    // ОПЕРАЦИИ С ПОЛЬЗОВАТЕЛЯМИ (Таблица users)
    // ============================================================
    bool registerUser(const std::string& username, const std::string& password_hash, bool is_admin = false);
    User authenticateUser(const std::string& username, const std::string& password_hash);
    bool userExists(const std::string& username);
    std::vector<User> getAllUsers();
    bool deleteUser(int user_id);
    bool isAdmin(int user_id);
    int getUserGamesCount(int user_id);
    bool changeUsername(int user_id, const std::string& new_username, const std::string& current_password);
    bool changePassword(int user_id, const std::string& new_password_hash);
    bool resetAdminCredentials();
    
    // ============================================================
    // ОПЕРАЦИИ С ЖАНРАМИ (Таблица genres)
    // ============================================================
    std::vector<Genre> getAllGenres();
    Genre getGenreById(int genre_id);
    Genre getGenreByName(const std::string& name);
    int addGenre(const std::string& name, const std::string& description = "");
    bool updateGenre(int genre_id, const std::string& name, const std::string& description);
    bool deleteGenre(int genre_id);
    
    // ============================================================
    // ОПЕРАЦИИ С ТЕГАМИ (Таблица tags)
    // ============================================================
    std::vector<Tag> getUserTags(int user_id);
    Tag getTagById(int tag_id);
    Tag getTagByName(const std::string& name, int user_id);
    int addTag(const std::string& name, int user_id, const std::string& color = "#808080");
    bool updateTag(int tag_id, const std::string& name, const std::string& color);
    bool deleteTag(int tag_id);
    
    // ============================================================
    // ОПЕРАЦИИ С ИГРАМИ (Таблица games)
    // ============================================================
    bool addGame(const Game& game);
    bool updateGame(const Game& game);
    bool deleteGame(int game_id, int user_id);
    bool deleteGameByName(const std::string& name, int user_id);
    std::vector<Game> getAllGames(int user_id);
    std::vector<Game> getFilteredGames(int user_id, const GameFilter& filter);
    Game getGameById(int game_id, int user_id);
    Game getGameByName(const std::string& name, int user_id);
    bool updateGameNotes(int game_id, int user_id, const std::string& notes);
    
    // ============================================================
    // ОПЕРАЦИИ СО СВЯЗЬЮ ИГРА-ТЕГ (Таблица game_tags)
    // ============================================================
    bool setGameTags(int game_id, const std::vector<int>& tag_ids);
    std::vector<int> getGameTagIds(int game_id);
    std::vector<Tag> getGameTags(int game_id);
    
    // ============================================================
    // СТАТИСТИКА И АНАЛИТИКА (Демо-запросы для ЛР6)
    // ============================================================
    GameStats getGameStats(int user_id);
    std::vector<GenreStats> getGenreStatistics(int user_id);  // Запрос с GROUP BY и агрегатными функциями
    std::vector<Game> getTopRatedGames(int user_id, int limit = 10);  // Запрос с ORDER BY и LIMIT
    std::vector<Game> getGamesWithTags(int user_id);  // Запрос с LEFT JOIN
    double getAverageRatingByGenre(int genre_id, int user_id);  // Запрос с AVG
    int countGamesAboveRating(int user_id, int min_rating);  // Запрос с HAVING
    std::vector<Game> searchGames(int user_id, const std::string& search_term);  // Запрос с LIKE
    std::vector<std::pair<std::string, int>> getTagUsageStats(int user_id);  // Статистика использования тегов
    std::vector<Game> getGamesCompletedByGenre(int user_id, int genre_id);  // Комбинированный запрос
    std::vector<Game> getUnplayedHighRatedGames(int user_id);  // Сложный запрос с подзапросом
    
    // ============================================================
    // ЭКСПОРТ/ИМПОРТ
    // ============================================================
    bool exportToBinaryFile(const std::string& filename, int user_id);
    bool exportFilteredToBinaryFile(const std::string& filename, int user_id, const GameFilter& filter);
    FileVerificationResult verifyBinaryFile(const std::string& filename);
    bool importFromBinaryFile(const std::string& filename, int user_id);
    std::vector<Game> readBinaryFile(const std::string& filename);
    
    // Получение последней ошибки
    std::string getLastError() const;
    static std::string getVerificationErrorText(FileVerificationResult result);
    
private:
    std::unique_ptr<pqxx::connection> conn_;
    std::string last_error_;
    
    std::string buildFilterCondition(const GameFilter& filter, int user_id);
    bool writeGamesToFile(const std::string& filename, const std::vector<Game>& games);
    void ensureAdminExists();
    void ensureDefaultGenres();
    std::string aggregateGameTags(int game_id);
};

} // namespace Temporium

#endif // DATABASE_MANAGER_H
