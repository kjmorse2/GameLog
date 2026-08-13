ALTER TABLE session_documents
    RENAME COLUMN html_content TO content;

-- statement-break

ALTER TABLE session_documents
    ADD format TEXT NOT NULL
        CHECK (format IN ('html', 'markdown')) default 'markdown';