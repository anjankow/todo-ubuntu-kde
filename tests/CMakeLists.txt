remove_definitions(-DQT_NO_CAST_FROM_ASCII -DQT_NO_CAST_TO_ASCII -DQT_NO_CAST_FROM_BYTEARRAY)

function(kdetodo_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE kdetodoui Qt5::Test)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;LC_ALL=C.UTF-8")
endfunction()

kdetodo_add_test(test_store)
kdetodo_add_test(test_storage)
kdetodo_add_test(test_newtaskedit)
kdetodo_add_test(test_labelsdialog)
kdetodo_add_test(test_mainwindow)
