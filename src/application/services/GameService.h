#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <QHash>
#include <QObject>
#include <QString>

#include "database/GameRepository.h"
#include "domain/Game.h"
#include "domain/query/GameQuery.h"

namespace gamelog::application::services
{
    /**
     * Application-facing game operations.
     *
     * Callers describe general searches with GameQuery or use the semantic
     * convenience methods. SQL remains entirely behind GameRepository.
     * The service also owns the in-memory index used by process matching.
     */
    class GameService : public QObject
    {
        Q_OBJECT

    public:
        explicit GameService(core::database::GameRepository& repository);

        ~GameService() override = default;

        [[nodiscard]] std::vector<core::domain::Game> search(const core::domain::query::GameQuery& query) const;

        [[nodiscard]] std::vector<core::domain::Game> listTrackedGames() const;

        [[nodiscard]] std::optional<core::domain::Game> findById(std::int64_t id) const;

        [[nodiscard]] std::optional<core::domain::Game> findByExecutableName(const QString& name) const;

        [[nodiscard]] std::optional<core::domain::Game> findByExecutablePath(const QString& path) const;

        [[nodiscard]] bool addGame(core::domain::Game& game);

        [[nodiscard]] bool updateGame(const core::domain::Game& game);

        [[nodiscard]] bool removeGame(std::int64_t id);

        /**
         * Rebuilds the process-matching indexes from the repository.
         */
        void syncGamesWithDatabase();

        /**
         * Read-only access to the service-owned process-matching indexes.
         * References remain valid until the next cache refresh.
         */
        [[nodiscard]] const QHash<std::uint32_t, core::domain::Game>& trackedSteamGames() const noexcept;

        [[nodiscard]] const QHash<QString, core::domain::Game>& trackedPathGames() const noexcept;

        [[nodiscard]] bool hasTrackedSteamGames() const noexcept;

    public
        slots:
        /**
         * Lists all games in the database and emits gamesFound with the results.
         */

        void listGames() const;

        signals:

        void gamesFound(std::vector<core::domain::Game> games) const;

    private:
        core::database::GameRepository& repository_;
        QHash<std::uint32_t, core::domain::Game> trackedSteamGames_;
        QHash<QString, core::domain::Game> trackedPathGames_;
    };
} // namespace gamelog::application::services
