#pragma once

#include <QWidget>

QT_BEGIN_NAMESPACE

namespace Ui
{
    class TextEditor;
}

QT_END_NAMESPACE

namespace gamelog::gui
{
    /**
     * Rich-text editor used for session notes, backed by Markdown.
     *
     * The widget edits formatted text through QTextEdit but reads and writes
     * Markdown via getMarkdown()/setMarkdown(), which is the format persisted in
     * Session::notes. Editing is gated by setAbleToEdit(): notes are writable
     * only while a session is live, so LiveWindow disables the editor once a
     * session ends.
     */
    class TextEditor : public QWidget
    {
        Q_OBJECT

    public:
        explicit TextEditor(QWidget* parent = nullptr);

        [[nodiscard]] QString getMarkdown();

        ~TextEditor() override;

    public
        Q_SLOTS :
        void setAbleToEdit(bool enabled);

    private
        Q_SLOTS :
        void applyHeading(int index);

        void toggleBold();

        void toggleItalic();

        void toggleStrikethrough();

        void addLink();

        void toggleBulletList();

        void updateToolbarState();

    private:
        [[nodiscard]] int currentHeadingLevel() const;

        [[nodiscard]] qreal headingFontSize(int level) const;

        Ui::TextEditor* ui{};
    };
} // namespace gamelog::gui
