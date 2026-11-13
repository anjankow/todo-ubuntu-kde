add_library(kdetodocore STATIC
    core/console.cpp
    core/palette.cpp
    core/postpone.cpp
    core/storage.cpp
    core/taskinput.cpp
    core/todostore.cpp
)
target_include_directories(kdetodocore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(kdetodocore PUBLIC Qt5::Core Qt5::Gui)

add_library(kdetodoapp STATIC
    app/appcontroller.cpp
    app/application.cpp
    app/settings.cpp
    app/tasklistmodel.cpp
    app/trayicon.cpp
    qml/qml.qrc
)
target_link_libraries(kdetodoapp PUBLIC
    kdetodocore
    Qt5::Widgets
    Qt5::Qml
    Qt5::Quick
    Qt5::QuickControls2
    KF5::CoreAddons
    KF5::I18n
    KF5::ConfigCore
)

add_executable(kdetodo main.cpp)
target_link_libraries(kdetodo PRIVATE kdetodoapp KF5::DBusAddons)

install(TARGETS kdetodo ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
