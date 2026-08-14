#pragma once

#include <QWidget>

QT_BEGIN_NAMESPACE

namespace Ui
{
    class TextEditor;
}

QT_END_NAMESPACE

class TextEditor:public QWidget
{
    Q_OBJECT

public:
    explicit TextEditor(QWidget* parent = nullptr);

    [[nodiscard]] QString getMarkdown();

    ~TextEditor() override;

public
    slots:



    void setAbleToEdit(bool enabled);

private
    slots:



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