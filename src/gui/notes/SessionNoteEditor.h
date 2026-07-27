#pragma once

#include <QWidget>

class QTextEdit;

namespace gamelog::gui::notes
{
class SessionNoteEditor final : public QWidget
{
public:
    explicit SessionNoteEditor(QWidget *parent = nullptr);

private:
    QTextEdit *m_textEdit{nullptr};
};
} // namespace gamelog::gui::notes
