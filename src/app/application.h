#pragma once

#include <QString>

class AppController;
class QQmlApplicationEngine;

// Shared by the application and its tests.
namespace Application
{
// Style and rendering settings; call before the QApplication is created.
void prepare();
// Loads the user interface for `controller`. Returns false when it failed.
bool load(QQmlApplicationEngine &engine, AppController *controller);
}
