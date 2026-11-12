#include "newtaskedit.h"

#include <KLocalizedString>

#include <QKeyEvent>

NewTaskEdit::NewTaskEdit(QWidget *parent)
    : QPlainTextEdit(parent)
{
    setObjectName(QStringLiteral("newTaskEdit"));
    setPlaceholderText(i18n("New task… (Shift+Enter adds a description, Enter saves)"));
    setTabChangesFocus(true);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(this, &QPlainTextEdit::textChanged, this, &NewTaskEdit::adjustHeight);
    adjustHeight();
}

void NewTaskEdit::parse(const QString &text, QString *title, QString *description)
{
    const QStringList lines = text.split(QLatin1Char('\n'));
    int first = 0;
    while (first < lines.size() && lines.at(first).trimmed().isEmpty()) {
        ++first;
    }
    if (first == lines.size()) {
        title->clear();
        description->clear();
        return;
    }
    *title = lines.at(first).trimmed();
    *description = lines.mid(first + 1).join(QLatin1Char('\n')).trimmed();
}

void NewTaskEdit::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (event->modifiers() & Qt::ShiftModifier) {
            insertPlainText(QStringLiteral("\n"));
        } else {
            submit();
        }
        event->accept();
        return;
    }
    QPlainTextEdit::keyPressEvent(event);
}

void NewTaskEdit::focusOutEvent(QFocusEvent *event)
{
    // A cleared (blank) draft is dropped; anything else stays as a draft.
    if (toPlainText().trimmed().isEmpty() && !toPlainText().isEmpty()) {
        clear();
    }
    QPlainTextEdit::focusOutEvent(event);
}

void NewTaskEdit::submit()
{
    QString title;
    QString description;
    parse(toPlainText(), &title, &description);
    clear();
    if (!title.isEmpty()) {
        Q_EMIT taskSubmitted(title, description);
    }
}

void NewTaskEdit::adjustHeight()
{
    const int lines = qMax(1, document()->blockCount());
    const int margins = 2 * (frameWidth() + static_cast<int>(document()->documentMargin()));
    setFixedHeight(fontMetrics().lineSpacing() * lines + margins + 2);
}
