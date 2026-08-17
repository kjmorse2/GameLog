ALTER TABLE games
    ADD COLUMN has_artwork2 INTEGER NOT NULL DEFAULT 0;

-- statement-break
UPDATE games
SET has_artwork2 = CASE
                       WHEN has_artwork NOT NULL AND has_artwork <> '' THEN 1
                       ELSE 0
    END;

-- statement-break
ALTER TABLE games
    DROP COLUMN has_artwork;

-- statement-break
ALTER TABLE games
    RENAME COLUMN has_artwork2 TO has_artwork;