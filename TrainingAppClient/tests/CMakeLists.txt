# qmltestrunner is a separate process and cannot see resources embedded in the
# application executable. Build a test import directory from the exact
# production QML files instead of maintaining test copies.
#
# Список файлов не поддерживается вручную: копируем весь каталог qml/ и
# генерируем qmldir, иначе тестовый модуль ломается при добавлении новых
# компонентов (например, страниц, на которые ссылается AppShell). Singletons
# перечислены явно: прагму `pragma Singleton` генератор qmldir не читает.
set(MOCK_IMPORT_ROOT "${CMAKE_CURRENT_BINARY_DIR}/imports")
set(MOCK_IMPORT_DIR "${MOCK_IMPORT_ROOT}/TrainingAppClient")

file(REMOVE_RECURSE "${MOCK_IMPORT_DIR}")
file(MAKE_DIRECTORY "${MOCK_IMPORT_DIR}")

set(SINGLETON_QML_NAMES
    ApiErrorText
    AuthViewModel
    BackRouter
    Demo
    ExercisesViewModel
    MockApi
    MockCatalog
    MockSession
    MockTokenStore
    PlansViewModel
    ProfileViewModel
    Spacing
    Theme
    TodayViewModel
    Typography
)

file(GLOB_RECURSE PRODUCTION_QML_FILES CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/../qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/../qml/*.qml"
)

set(MOCK_IMPORT_QMLDIR "module TrainingAppClient\n")
set(MOCK_IMPORT_TYPE_NAMES)

foreach(qml_file IN LISTS PRODUCTION_QML_FILES)
    get_filename_component(qml_name "${qml_file}" NAME_WE)
    if (qml_name IN_LIST MOCK_IMPORT_TYPE_NAMES)
        message(FATAL_ERROR
            "Duplicate QML type name '${qml_name}' in production sources. "
            "The test module cannot flatten both files into one URI."
        )
    endif()
    list(APPEND MOCK_IMPORT_TYPE_NAMES "${qml_name}")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/../qml/${qml_file}"
        "${MOCK_IMPORT_DIR}/${qml_name}.qml"
        COPYONLY
    )

    if (qml_name IN_LIST SINGLETON_QML_NAMES)
        string(APPEND MOCK_IMPORT_QMLDIR "singleton ${qml_name} 1.0 ${qml_name}.qml\n")
    else()
        string(APPEND MOCK_IMPORT_QMLDIR "${qml_name} 1.0 ${qml_name}.qml\n")
    endif()
endforeach()

file(WRITE "${MOCK_IMPORT_DIR}/qmldir" "${MOCK_IMPORT_QMLDIR}")

qt_add_executable(tst_App_Context
    cpp/tst_App_Context.cpp
)
target_link_libraries(tst_App_Context PRIVATE TrainingAppCore Qt6::Test)
add_test(NAME app_context COMMAND tst_App_Context)
set_tests_properties(app_context PROPERTIES TIMEOUT 20)

qt_add_executable(tst_Sql_Database
    cpp/tst_Sql_Database.cpp
)
target_link_libraries(tst_Sql_Database PRIVATE TrainingAppCore Qt6::Test)
add_test(NAME sql_database COMMAND tst_Sql_Database)
set_tests_properties(sql_database PROPERTIES TIMEOUT 20)

qt_add_executable(tst_Token_Store
    cpp/tst_Token_Store.cpp
)
target_link_libraries(tst_Token_Store PRIVATE TrainingAppCore Qt6::Test)
add_test(NAME token_store COMMAND tst_Token_Store)
set_tests_properties(token_store PROPERTIES TIMEOUT 20)

qt_add_executable(tst_Auth_Session
    cpp/tst_Auth_Session.cpp
)
target_link_libraries(tst_Auth_Session PRIVATE TrainingAppCore TrainingAppTestSupport Qt6::Test)
add_test(NAME auth_session COMMAND tst_Auth_Session)
set_tests_properties(auth_session PROPERTIES TIMEOUT 30)

add_library(TrainingAppTestSupport STATIC
    Support/Test_Http_Server.h
    Support/Test_Http_Server.cpp
)
target_include_directories(TrainingAppTestSupport PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(TrainingAppTestSupport PUBLIC Qt6::Core Qt6::Network)

qt_add_executable(tst_Test_Http_Server
    cpp/tst_Test_Http_Server.cpp
)
target_link_libraries(tst_Test_Http_Server PRIVATE TrainingAppTestSupport Qt6::Test)
add_test(NAME test_http_server COMMAND tst_Test_Http_Server)
set_tests_properties(test_http_server PROPERTIES TIMEOUT 20)

qt_add_executable(tst_Http_Client
    cpp/tst_Http_Client.cpp
)
target_link_libraries(tst_Http_Client PRIVATE TrainingAppCore TrainingAppTestSupport Qt6::Test)
add_test(NAME http_client COMMAND tst_Http_Client)
set_tests_properties(http_client PROPERTIES TIMEOUT 30)

# Автономные экраны входа и регистрации (issue 2) на пяти компоновках каждая.
foreach(auth_screen IN ITEMS auth-login auth-signup)
    foreach(auth_viewport IN ITEMS 360 390 480 frame dark)
        set(auth_test_name "ui_smoke_${auth_screen}_${auth_viewport}")
        add_test(
            NAME "${auth_test_name}"
            COMMAND "${CMAKE_COMMAND}"
                "-DAPP_EXECUTABLE=$<TARGET_FILE:appTrainingAppClient>"
                "-DQT_RUNTIME_DIR=$<TARGET_FILE_DIR:Qt6::Core>"
                "-DSCREEN=${auth_screen}"
                "-DVIEWPORT=${auth_viewport}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_ui_smoke.cmake"
        )
        set_tests_properties("${auth_test_name}" PROPERTIES
            PASS_REGULAR_EXPRESSION "problems=0"
            FAIL_REGULAR_EXPRESSION "TEXT-WIDTH|TEXT-HEIGHT|OVERFLOW-X|ZERO-WIDTH|QQmlApplicationEngine failed|ReferenceError|TypeError|is not a type"
            TIMEOUT 20
        )
    endforeach()
endforeach()

if (TARGET Qt6::qmltestrunner)
    set(QML_TEST_RUNNER "$<TARGET_FILE:Qt6::qmltestrunner>")
else()
    find_program(QML_TEST_RUNNER qmltestrunner REQUIRED)
endif()

add_test(
    NAME qml_tests
    COMMAND "${QML_TEST_RUNNER}"
        -input "${CMAKE_CURRENT_SOURCE_DIR}/qml"
        -import "${MOCK_IMPORT_ROOT}"
        -platform offscreen
)

set_tests_properties(qml_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QML_DISABLE_DISK_CACHE=1;QT_QUICK_CONTROLS_STYLE=Basic"
    TIMEOUT 60
)

# These tests load the production QML module through the real executable and
# fail on any layout problem reported by Main.qml's diagnostic mode.
foreach(tab_index RANGE 0 3)
    add_test(
        NAME "ui_smoke_tab_${tab_index}"
        COMMAND "${CMAKE_COMMAND}"
            "-DAPP_EXECUTABLE=$<TARGET_FILE:appTrainingAppClient>"
            "-DQT_RUNTIME_DIR=$<TARGET_FILE_DIR:Qt6::Core>"
            "-DTAB_INDEX=${tab_index}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_ui_smoke.cmake"
    )
    set_tests_properties("ui_smoke_tab_${tab_index}" PROPERTIES
        PASS_REGULAR_EXPRESSION "problems=0"
        FAIL_REGULAR_EXPRESSION "TEXT-WIDTH|TEXT-HEIGHT|OVERFLOW-X|ZERO-WIDTH|QQmlApplicationEngine failed|ReferenceError|TypeError|is not a type"
        TIMEOUT 20
    )
endforeach()

# Гостевой режим: вкладка «Профиль» доступна без сессии, вход открывается
# кнопкой «Войти» (та же логика, что и у делегата вкладки).
foreach(auth_viewport IN ITEMS 360 390 480 frame dark)
    set(guest_test_name "ui_smoke_profile_unauth_${auth_viewport}")
    add_test(
        NAME "${guest_test_name}"
        COMMAND "${CMAKE_COMMAND}"
            "-DAPP_EXECUTABLE=$<TARGET_FILE:appTrainingAppClient>"
            "-DQT_RUNTIME_DIR=$<TARGET_FILE_DIR:Qt6::Core>"
            "-DTAB_INDEX=3"
            "-DVIEWPORT=${auth_viewport}"
            "-DUNAUTH=1"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_ui_smoke.cmake"
    )
    set_tests_properties("${guest_test_name}" PROPERTIES
        PASS_REGULAR_EXPRESSION "problems=0"
        FAIL_REGULAR_EXPRESSION "TEXT-WIDTH|TEXT-HEIGHT|OVERFLOW-X|ZERO-WIDTH|QQmlApplicationEngine failed|ReferenceError|TypeError|is not a type"
        TIMEOUT 20
    )

    # Экран входа, открытый как из «Профиля»: сессии нет, экран закрываемый.
    set(guest_auth_test_name "ui_smoke_auth_from_profile_${auth_viewport}")
    add_test(
        NAME "${guest_auth_test_name}"
        COMMAND "${CMAKE_COMMAND}"
            "-DAPP_EXECUTABLE=$<TARGET_FILE:appTrainingAppClient>"
            "-DQT_RUNTIME_DIR=$<TARGET_FILE_DIR:Qt6::Core>"
            "-DVIEWPORT=${auth_viewport}"
            "-DOPEN_AUTH=1"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_ui_smoke.cmake"
    )
    set_tests_properties("${guest_auth_test_name}" PROPERTIES
        PASS_REGULAR_EXPRESSION "problems=0"
        FAIL_REGULAR_EXPRESSION "TEXT-WIDTH|TEXT-HEIGHT|OVERFLOW-X|ZERO-WIDTH|QQmlApplicationEngine failed|ReferenceError|TypeError|is not a type"
        TIMEOUT 20
    )
endforeach()
