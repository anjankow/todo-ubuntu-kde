remove_definitions(-DQT_NO_CAST_FROM_ASCII -DQT_NO_CAST_TO_ASCII -DQT_NO_CAST_FROM_BYTEARRAY)

function(kdetodo_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE kdetodoapp Qt5::Test)
    add_test(NAME ${name} COMMAND ${name})
    # No display and no GPU are needed: offscreen platform, software rendering.
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;LC_ALL=C.UTF-8")
endfunction()

kdetodo_add_test(test_store)
kdetodo_add_test(test_storage)
kdetodo_add_test(test_controller)
kdetodo_add_test(test_ui)

kdetodo_add_test(test_console)
add_executable(console_probe console_probe.cpp)
target_link_libraries(console_probe PRIVATE kdetodocore)
add_dependencies(test_console console_probe)
target_compile_definitions(test_console PRIVATE KDETODO_CONSOLE_PROBE="$<TARGET_FILE:console_probe>")
