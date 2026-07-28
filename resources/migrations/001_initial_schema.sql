CREATE TABLE games (
    id INTEGER PRIMARY KEY,
    title TEXT NOT NULL,
    executable_path TEXT
);

CREATE TABLE sessions (
    id INTEGER PRIMARY KEY,
    game_id INTEGER NOT NULL,
    start_time_utc TEXT NOT NULL,
    end_time_utc TEXT,
    FOREIGN KEY (game_id) REFERENCES games(id)
);