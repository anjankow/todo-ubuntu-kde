#pragma once

#include <QPlainTextEdit>

// Input for a new task. The first line is the title; Shift+Enter starts the
// description on the following lines; Enter creates the task.
class NewTaskEdit : public QPlainTextEdit
{
    Q_OBJECT
public:
    explicit NewTaskEdit(QWidget *parent = nullptr);

    // Splits raw input into title (first non-blank line) and description.
    static void parse(const QString &text, QString *title, QString *description);

Q_SIGNALS:
    void taskSubmitted(const QString &title, const QString &description);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    void submit();
    void adjustHeight();
};
