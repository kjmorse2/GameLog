#include "notes/SessionNoteEditor.h"

#include <QTextEdit>
#include <QVBoxLayout>

namespace gamelog::gui::notes
{
SessionNoteEditor::SessionNoteEditor(QWidget *parent)
    : QWidget(parent)
    , m_textEdit(new QTextEdit(this))
{
    auto *layout = new QVBoxLayout(this);

    m_textEdit->setAcceptRichText(true);
    m_textEdit->setHtml(QStringLiteral("<p><em>Session notes will appear here.</em></p>"));

    // TODO: Add autosave, formatting controls, and timestamp insertion.
    // TODO: Persist canonical HTML notes through session-document storage.
    layout->addWidget(m_textEdit);
}
} // namespace gamelog::gui::notes
