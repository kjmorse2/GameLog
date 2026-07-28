#include "SessionRepository.h"
#include "domain/Session.h"
#include <QtCore/qloggingcategory.h>
#include <QSqlQuery>

namespace gamelog::core::database
{
    SessionRepository::SessionRepository(QSqlDatabase database)
        : database_(std::move(database))
    {
    }

    std::vector<domain::Session> SessionRepository::listSessionsForGame(int gameId)
    {
        QSqlQuery query(database_);
        query.prepare("SELECT id, game_id, start_timestamp, end_timestamp, tracked_duration, source, status FROM sessions WHERE game_id = :game_id");
        query.bindValue(":game_id", gameId);
        query.exec();

        std::vector<domain::Session> sessions;
        while (query.next()) {
            domain::Session session;
            session.id = query.value("id").toInt();
            session.gameId = query.value("game_id").toInt();
            session.startTimestamp = query.value("start_timestamp").toDateTime();
            session.endTimestamp = query.value("end_timestamp").toDateTime();
            session.trackedDuration = std::chrono::seconds(query.value("tracked_duration").toInt());
            session.source = domain::sessionSourceFromString(query.value("source").toString());
            session.status = domain::sessionStatusFromString(query.value("status").toString());
            sessions.push_back(session);
        }

        return sessions;
    }

}