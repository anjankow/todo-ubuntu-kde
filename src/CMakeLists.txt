add_library(kdetodocore STATIC
    core/palette.cpp
    core/postpone.cpp
    core/storage.cpp
    core/todostore.cpp
)
target_include_directories(kdetodocore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(kdetodocore PUBLIC Qt5::Core Qt5::Gui)

add_library(kdetodoui STATIC
    ui/labelsdialog.cpp
    ui/mainwindow.cpp
    ui/newtaskedit.cpp
    ui/settings.cpp
    ui/taskdialog.cpp
    ui/tasklistview.cpp
)
target_link_libraries(kdetodoui PUBLIC
    kdetodocore
    Qt5::Widgets
    KF5::CoreAddons
    KF5::I18n
    KF5::ConfigCore
    KF5::WidgetsAddons
    KF5::XmlGui
)

add_executable(kdetodo main.cpp)
target_link_libraries(kdetodo PRIVATE kdetodoui)

install(TARGETS kdetodo ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
