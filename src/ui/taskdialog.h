#pragma once

#include <QDialog>

class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class TodoStore;

// The task card: shows and edits title, description and labels of a task.
class TaskDialog : public QDialog
{
    Q_OBJECT
public:
    TaskDialog(TodoStore *store, const QString &taskId, QWidget *parent = nullptr);

    QString taskId() const { return m_taskId; }
    QLineEdit *titleEdit() const { return m_title; }
    QPlainTextEdit *descriptionEdit() const { return m_description; }
    QListWidget *labelList() const { return m_labels; }

    void accept() override;

private:
    TodoStore *m_store;
    QString m_taskId;
    QLineEdit *m_title;
    QPlainTextEdit *m_description;
    QListWidget *m_labels;
};
