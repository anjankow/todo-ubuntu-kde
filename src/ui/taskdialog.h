#pragma once

#include "core/postpone.h"

#include <QDateTime>
#include <QDialog>
#include <QMap>

#include <functional>

class QAction;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class TodoStore;

// The task card: shows and edits title, description and labels of a task.
// Its Complete, Postpone and Cancel task buttons save the edits, change the
// state at once and close the card.
class TaskDialog : public QDialog
{
    Q_OBJECT
public:
    struct Hooks {
        std::function<QDateTime()> now;
        std::function<QDateTime()> askPostponeTime; // invalid = cancelled
    };

    TaskDialog(TodoStore *store, const QString &taskId, Hooks hooks, QWidget *parent = nullptr);

    QString taskId() const { return m_taskId; }
    QLineEdit *titleEdit() const { return m_title; }
    QPlainTextEdit *descriptionEdit() const { return m_description; }
    QListWidget *labelList() const { return m_labels; }
    QPushButton *completeButton() const { return m_complete; }
    QPushButton *cancelTaskButton() const { return m_cancelTask; }
    QPushButton *postponeButton() const { return m_postpone; }
    QAction *postponeAction(PostponePreset preset) const { return m_postponePresets.value(preset); }
    QAction *postponeCustomAction() const { return m_postponeCustom; }

    void accept() override;

private:
    bool applyEdits();
    void postpone(const QDateTime &until);

    TodoStore *m_store;
    QString m_taskId;
    Hooks m_hooks;
    QLineEdit *m_title;
    QPlainTextEdit *m_description;
    QListWidget *m_labels;
    QPushButton *m_complete;
    QPushButton *m_cancelTask;
    QPushButton *m_postpone;
    QAction *m_postponeCustom = nullptr;
    QMap<PostponePreset, QAction *> m_postponePresets;
};
