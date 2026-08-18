ALTER TABLE games
    ADD COLUMN has_artwork2 INTEGER NOT NULL DEFAULT 0
        CHECK (has_artwork2 IN (0, 1));
-- statement-break
UPDATE games
SET has_artwork2 = CASE
                       WHEN artwork_path IS NOT NULL
                           AND TRIM(artwork_path, char(9) || char(10) || char(11) || char(12) || char(13) || ' ') <> ''
                           THEN 1
                       ELSE 0
    END;
-- statement-break
ALTER TABLE games
    DROP COLUMN artwork_path;
-- statement-break
ALTER TABLE games
    RENAME COLUMN has_artwork2 TO has_artwork;
