CREATE TABLE games
(
    id INTEGER PRIMARY KEY,
    title TEXT NOT NULL,
    executable_path TEXT,
    executable_name TEXT,
    steam_app_id INTEGER UNIQUE,
    artwork_path TEXT,
    tracking_enabled INTEGER NOT NULL DEFAULT 1
        CHECK (tracking_enabled IN (0, 1))
);

-- statement-break

CREATE TABLE sessions
(
    id INTEGER PRIMARY KEY,
    game_id INTEGER NOT NULL,
    start_timestamp_utc TEXT NOT NULL,
    end_timestamp_utc TEXT,
    tracked_duration_seconds INTEGER NOT NULL DEFAULT 0
        CHECK (tracked_duration_seconds >= 0),
    source TEXT NOT NULL
        CHECK (source IN ('automatic', 'manual')),
    status TEXT NOT NULL
        CHECK (
            status IN (
                'active',
                'completed',
                'interrupted'
            )
        ),

    FOREIGN KEY (game_id)
        REFERENCES games(id)
        ON DELETE CASCADE
);

-- statement-break

CREATE TABLE session_documents
(
    session_id INTEGER PRIMARY KEY,
    html_content TEXT NOT NULL DEFAULT '',
    last_saved_timestamp_utc TEXT,

    FOREIGN KEY (session_id)
        REFERENCES sessions(id)
        ON DELETE CASCADE
);

CREATE TABLE schema_migrations
(
    version INTEGER PRIMARY KEY,
    name TEXT NOT NULL UNIQUE,
    applied_at_utc TEXT NOT NULL
);