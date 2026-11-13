#pragma once

#include "app/appcontroller.h"
#include "app/settings.h"
#include "core/storage.h"
#include "core/todostore.h"

#include <QTemporaryDir>

#include <memory>

inline const QDateTime fixedNow(QDate(2026, 11, 12), QTime(10, 0));

// A controller working on a temporary folder with a settable clock.
struct TestEnv {
    QTemporaryDir dir;
    QString configFile;
    QDateTime now = fixedNow;
    std::unique_ptr<AppController> app;

    TestEnv()
    {
        configFile = dir.filePath("kdetodorc");
        Settings(configFile).setStorageDir(dir.path());
        open();
    }
    virtual ~TestEnv() = default;

    // (Re)creates the controller, like starting the application again.
    virtual void open()
    {
        app = std::make_unique<AppController>(configFile, [this] {
            return now;
        });
    }

    TodoStore *store() const { return app->store(); }
    TaskListModel *list(TaskState state = TaskState::Active) const { return app->model(state); }

    QString add(const QString &title, const QString &description = {}) { return store()->addTask(title, description); }

    QStringList shown(TaskState state = TaskState::Active) const
    {
        QStringList result;
        TaskListModel *model = list(state);
        for (int i = 0; i < model->rowCount(); ++i) {
            result.append(model->data(model->index(i), TaskListModel::TitleRole).toString());
        }
        return result;
    }

    QStringList savedTitles() const
    {
        TodoStore disk;
        Storage::load(disk, app->storagePath());
        QStringList result;
        for (const Task &t : disk.tasks()) {
            result.append(t.title);
        }
        return result;
    }
};
