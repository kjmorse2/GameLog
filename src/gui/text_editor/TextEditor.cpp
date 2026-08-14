#include "TextEditor.h"

#include "ui_texteditor.h"

#include <QComboBox>
#include <QFont>
#include <QInputDialog>
#include <QLineEdit>
#include <QPalette>
#include <QSignalBlocker>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextEdit>
#include <QTextList>
#include <QToolButton>

TextEditor::TextEditor(QWidget* parent): QWidget{parent},
                                         ui{new Ui::TextEditor}
{
    ui->setupUi(this);

    connect(ui->headingComboBox, &QComboBox::currentIndexChanged, this, &TextEditor::applyHeading);

    connect(ui->boldButton, &QToolButton::clicked, this, &TextEditor::toggleBold);

    connect(ui->italicButton, &QToolButton::clicked, this, &TextEditor::toggleItalic);

    connect(ui->strikethroughButton, &QToolButton::clicked, this, &TextEditor::toggleStrikethrough);

    connect(ui->linkButton, &QToolButton::clicked, this, &TextEditor::addLink);

    connect(ui->bulletListButton, &QToolButton::clicked, this, &TextEditor::toggleBulletList);

    connect(
            ui->zoomOutButton,
            &QToolButton::clicked,
            this,
            [this]
            {
                ui->textEdit->zoomOut(1);
            }
           );

    connect(
            ui->zoomInButton,
            &QToolButton::clicked,
            this,
            [this]
            {
                ui->textEdit->zoomIn(1);
            }
           );

    connect(ui->textEdit, &QTextEdit::cursorPositionChanged, this, &TextEditor::updateToolbarState);

    connect(
            ui->textEdit,
            &QTextEdit::currentCharFormatChanged,
            this,
            [this](const QTextCharFormat&)
            {
                updateToolbarState();
            }
           );

    updateToolbarState();

    setAbleToEdit(false);
}

TextEditor::~TextEditor()
{
    delete ui;
}

void TextEditor::setAbleToEdit(bool enabled)
{
    ui->textEdit->setDisabled(!enabled);
}

void TextEditor::toggleBold()
{
    QTextCharFormat format;

    format.setFontWeight(ui->boldButton->isChecked() ? QFont::Bold : QFont::Normal);

    ui->textEdit->mergeCurrentCharFormat(format);
    ui->textEdit->setFocus();
}

void TextEditor::toggleItalic()
{
    QTextCharFormat format;

    format.setFontItalic(ui->italicButton->isChecked());

    ui->textEdit->mergeCurrentCharFormat(format);
    ui->textEdit->setFocus();
}

void TextEditor::toggleStrikethrough()
{
    QTextCharFormat format;

    format.setFontStrikeOut(ui->strikethroughButton->isChecked());

    ui->textEdit->mergeCurrentCharFormat(format);
    ui->textEdit->setFocus();
}

void TextEditor::updateToolbarState()
{
    const QTextCharFormat format = ui->textEdit->currentCharFormat();

    ui->boldButton->setChecked(format.fontWeight() >= QFont::Bold);

    ui->italicButton->setChecked(format.fontItalic());

    ui->strikethroughButton->setChecked(format.fontStrikeOut());

    ui->bulletListButton->setChecked(ui->textEdit->textCursor().currentList() != nullptr);

    const int headingLevel = currentHeadingLevel();

    const QSignalBlocker blocker{ui->headingComboBox};

    if(headingLevel >= 0 && headingLevel <= 3)
    {
        // Conveniently, our combo indices correspond directly
        // to heading levels:
        //
        // 0 = Normal
        // 1 = H1
        // 2 = H2
        // 3 = H3
        ui->headingComboBox->setCurrentIndex(headingLevel);
    }
    else
    {
        ui->headingComboBox->setCurrentIndex(0);
    }
}

void TextEditor::applyHeading(int index)
{
    const int level = index;

    QTextCursor cursor = ui->textEdit->textCursor();

    cursor.beginEditBlock();

    QTextBlockFormat blockFormat;
    blockFormat.setHeadingLevel(level);

    cursor.mergeBlockFormat(blockFormat);

    QTextCharFormat charFormat;
    charFormat.setFontPointSize(headingFontSize(level));

    charFormat.setFontWeight(level > 0 ? QFont::Bold : QFont::Normal);

    cursor.mergeBlockCharFormat(charFormat);

    cursor.endEditBlock();

    ui->textEdit->setTextCursor(cursor);
    ui->textEdit->setFocus();
}

void TextEditor::addLink()
{
    QTextCursor cursor = ui->textEdit->textCursor();

    bool accepted = false;

    QString initialValue;

    // If the user selected what looks like a URL, use it as
    // the initial value in the URL dialog.
    if(cursor.hasSelection())
    {
        const QString selectedText = cursor.selectedText();

        if(selectedText.startsWith(QStringLiteral("http://")) || selectedText.startsWith(QStringLiteral("https://")))
        {
            initialValue = selectedText;
        }
    }

    if(initialValue.isEmpty())
    {
        initialValue = QStringLiteral("https://");
    }

    const QString url = QInputDialog::getText(this, tr("Insert Link"), tr("URL:"), QLineEdit::Normal, initialValue, &accepted);

    if(!accepted || url.trimmed().isEmpty())
    {
        return;
    }

    QTextCharFormat format;
    format.setAnchor(true);
    format.setAnchorHref(url);
    format.setFontUnderline(true);

    format.setForeground(ui->textEdit->palette().color(QPalette::Link));

    if(cursor.hasSelection())
    {
        // Turn the existing selected text into a hyperlink.
        cursor.mergeCharFormat(format);
    }
    else
    {
        // No selection: insert the URL itself as the linked text.
        cursor.insertText(url, format);
    }

    ui->textEdit->setTextCursor(cursor);
    ui->textEdit->setFocus();
}

void TextEditor::toggleBulletList()
{
    QTextCursor cursor = ui->textEdit->textCursor();

    QTextList* list = cursor.currentList();

    if(list)
    {
        QTextBlockFormat blockFormat = cursor.blockFormat();
        blockFormat.setObjectIndex(-1);
        cursor.setBlockFormat(blockFormat);
    }
    else
    {
        QTextListFormat listFormat;
        listFormat.setStyle(QTextListFormat::ListDisc);
        cursor.createList(listFormat);
    }

    ui->textEdit->setTextCursor(cursor);
}

int TextEditor::currentHeadingLevel() const
{
    const QTextCursor cursor = ui->textEdit->textCursor();
    return cursor.blockFormat().headingLevel();
}


qreal TextEditor::headingFontSize(int level) const
{
    qreal baseSize = ui->textEdit->font().pointSizeF();

    // Some fonts may not have a valid point size, e.g. if they
    // are specified in pixels. Give ourselves a sane fallback.
    if(baseSize <= 0.0)
    {
        baseSize = 11.0;
    }

    switch(level)
    {
        case 1 :
            return baseSize * 1.8;

        case 2 :
            return baseSize * 1.5;

        case 3 :
            return baseSize * 1.25;

        default :
            return baseSize;
    }
}

QString TextEditor::getMarkdown()
{
    return ui->textEdit->toMarkdown();
}
