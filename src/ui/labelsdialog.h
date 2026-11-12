#pragma once

#include <QDialog>

class QButtonGroup;
class QLineEdit;
class QListWidget;
class QPushButton;
class TodoStore;

// Manages the user-defined labels: name plus one of the 12 palette colors.
class LabelsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LabelsDialog(TodoStore *store, QWidget *parent = nullptr);

    QListWidget *labelList() const { return m_list; }
    QLineEdit *nameEdit() const { return m_name; }
    QButtonGroup *colorButtons() const { return m_colors; }
    QPushButton *addButton() const { return m_add; }
    QPushButton *saveButton() const { return m_save; }
    QPushButton *deleteButton() const { return m_delete; }

private:
    void refresh();
    void showSelected();
    QString selectedId() const;

    TodoStore *m_store;
    QListWidget *m_list;
    QLineEdit *m_name;
    QButtonGroup *m_colors;
    QPushButton *m_add;
    QPushButton *m_save;
    QPushButton *m_delete;
};
