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
    Typography
)

file(GLOB_RECURSE PRODUCTION_QML_FILES CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_SOURCE_DIR}/qml"
    "${CMAKE_SOURCE_DIR}/qml/*.qml"
)

set(MOCK_IMPORT_QMLDIR "module TrainingAppClient\n")
set(MOCK_IMPORT_TYPE_NAMES)
set(PRODUCTION_QML_ABS_FILES)

foreach(qml_file IN LISTS PRODUCTION_QML_FILES)
    get_filename_component(qml_name "${qml_file}" NAME_WE)
    if (qml_name IN_LIST MOCK_IMPORT_TYPE_NAMES)
        message(FATAL_ERROR
            "Duplicate QML type name '${qml_name}' in production sources. "
            "The test module cannot flatten both files into one URI."
        )
    endif()
    list(APPEND MOCK_IMPORT_TYPE_NAMES "${qml_name}")
    list(APPEND PRODUCTION_QML_ABS_FILES "${CMAKE_SOURCE_DIR}/qml/${qml_file}")
    configure_file(
        "${CMAKE_SOURCE_DIR}/qml/${qml_file}"
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

# Копии выше делаются на этапе конфигурации, а CONFIGURE_DEPENDS следит только
# за списком файлов: правка содержимого уже скопированного компонента
# переконфигурацию не запускала, и QML-тесты прогонялись по устаревшим копиям —
# зелёный прогон ничего не говорил о текущем `qml/`. Явная зависимость от самих
# файлов заставляет CMake переконфигурировать проект при любой правке
# production-QML, и копии обновляются вместе с исходниками.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${PRODUCTION_QML_ABS_FILES})

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

qt_add_executable(tst_Program_Model
    cpp/tst_Program_Model.cpp
)
target_link_libraries(tst_Program_Model PRIVATE TrainingAppCore Qt6::Test)
add_test(NAME program_model COMMAND tst_Program_Model)
set_tests_properties(program_model PROPERTIES TIMEOUT 20)

qt_add_executable(tst_Program_Service
    cpp/tst_Program_Service.cpp
)
target_link_libraries(tst_Program_Service PRIVATE TrainingAppCore Qt6::Test)
add_test(NAME program_service COMMAND tst_Program_Service)
set_tests_properties(program_service PROPERTIES
    TIMEOUT 30
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:Qt6::Core>;$ENV{PATH}"
)

# Сидер демо-программы. Отдельный набор: он пишет в БД и проверяет, что данные,
# на которых смотрят экраны, вообще читаются обратно.
qt_add_executable(tst_Demo_Seeder
    cpp/tst_Demo_Seeder.cpp
)
target_link_libraries(tst_Demo_Seeder PRIVATE TrainingAppCore Qt6::Test)
add_test(NAME demo_seeder COMMAND tst_Demo_Seeder)
set_tests_properties(demo_seeder PROPERTIES
    TIMEOUT 30
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:Qt6::Core>;$ENV{PATH}"
)

qt_add_executable(tst_Rule_Engine
    cpp/tst_Rule_Engine.cpp
)
target_link_libraries(tst_Rule_Engine PRIVATE TrainingAppCore Qt6::Test)
add_test(NAME rule_engine COMMAND tst_Rule_Engine)
set_tests_properties(rule_engine PROPERTIES TIMEOUT 20)

qt_add_executable(tst_List_Model
    cpp/tst_List_Model.cpp
)
target_link_libraries(tst_List_Model PRIVATE TrainingAppCore Qt6::Test)
add_test(NAME list_model COMMAND tst_List_Model)
set_tests_properties(list_model PROPERTIES TIMEOUT 20)

qt_add_executable(tst_Program_Rows
    cpp/tst_Program_Rows.cpp
)
target_link_libraries(tst_Program_Rows PRIVATE TrainingAppCore Qt6::Test)
add_test(NAME program_rows COMMAND tst_Program_Rows)
set_tests_properties(program_rows PROPERTIES TIMEOUT 20)

qt_add_executable(tst_Program_Rules_VM cpp/tst_Program_Rules_VM.cpp)
target_link_libraries(tst_Program_Rules_VM PRIVATE TrainingAppCore Qt6::Test)
add_test(NAME program_rules_vm COMMAND tst_Program_Rules_VM)

qt_add_executable(tst_Program_Stats_VM
    cpp/tst_Program_Stats_VM.cpp
)
target_link_libraries(tst_Program_Stats_VM PRIVATE TrainingAppCore Qt6::Test)
add_test(NAME program_stats_vm COMMAND tst_Program_Stats_VM)
set_tests_properties(program_stats_vm PROPERTIES TIMEOUT 30)

qt_add_executable(tst_Today_VM cpp/tst_Today_VM.cpp)
target_link_libraries(tst_Today_VM PRIVATE TrainingAppCore Qt6::Test)
add_test(NAME today_vm COMMAND tst_Today_VM)
set_tests_properties(today_vm PROPERTIES TIMEOUT 30)

qt_add_executable(tst_Workout_Repository
    cpp/tst_Workout_Repository.cpp
)
target_link_libraries(tst_Workout_Repository PRIVATE TrainingAppCore Qt6::Test)
add_test(NAME workout_repository COMMAND tst_Workout_Repository)
set_tests_properties(workout_repository PROPERTIES TIMEOUT 30)

qt_add_executable(tst_Program_VM
    cpp/tst_Program_VM.cpp
)
target_link_libraries(tst_Program_VM PRIVATE TrainingAppCore Qt6::Test)
add_test(NAME program_vm COMMAND tst_Program_VM)
set_tests_properties(program_vm PROPERTIES TIMEOUT 30)

qt_add_executable(tst_Error_State
    cpp/tst_Error_State.cpp
)
target_link_libraries(tst_Error_State PRIVATE TrainingAppCore Qt6::Test)
add_test(NAME error_state COMMAND tst_Error_State)
set_tests_properties(error_state PROPERTIES TIMEOUT 20)

qt_add_executable(tst_Progress_Calculator
    cpp/tst_Progress_Calculator.cpp
)
target_link_libraries(tst_Progress_Calculator PRIVATE TrainingAppCore Qt6::Test)
add_test(NAME progress_calculator COMMAND tst_Progress_Calculator)
set_tests_properties(progress_calculator PROPERTIES TIMEOUT 20)

# Авторасчёт КБЖУ (спринт S7). Отдельный набор, а не проверка внутри
# tst_Progress_Calculator: формулы КБЖУ не имеют ничего общего с расчётом
# прогресса, и смешивать их в одном бинарнике означало бы тащить в тест
# лишние зависимости.
qt_add_executable(tst_Macro_Calculator
    cpp/tst_Macro_Calculator.cpp
)
target_link_libraries(tst_Macro_Calculator PRIVATE TrainingAppCore Qt6::Test)
add_test(NAME macro_calculator COMMAND tst_Macro_Calculator)
set_tests_properties(macro_calculator PROPERTIES TIMEOUT 20)

qt_add_executable(tst_Macro_VM
    cpp/tst_Macro_VM.cpp
)
target_link_libraries(tst_Macro_VM PRIVATE TrainingAppCore Qt6::Test)
add_test(NAME macro_vm COMMAND tst_Macro_VM)
set_tests_properties(macro_vm PROPERTIES TIMEOUT 20)

qt_add_executable(tst_Program_Repository
    cpp/tst_Program_Repository.cpp
)
target_link_libraries(tst_Program_Repository PRIVATE TrainingAppCore Qt6::Test)
add_test(NAME program_repository COMMAND tst_Program_Repository)
set_tests_properties(program_repository PROPERTIES TIMEOUT 30)

qt_add_executable(tst_Rule_Repository
    cpp/tst_Rule_Repository.cpp
)
target_link_libraries(tst_Rule_Repository PRIVATE TrainingAppCore Qt6::Test)
add_test(NAME rule_repository COMMAND tst_Rule_Repository)
set_tests_properties(rule_repository PROPERTIES TIMEOUT 30)

qt_add_executable(tst_Measurement_Repository
    cpp/tst_Measurement_Repository.cpp
)
target_link_libraries(tst_Measurement_Repository PRIVATE TrainingAppCore Qt6::Test)
add_test(NAME measurement_repository COMMAND tst_Measurement_Repository)
set_tests_properties(measurement_repository PROPERTIES TIMEOUT 30)

qt_add_executable(tst_Stats_Repository
    cpp/tst_Stats_Repository.cpp
)
target_link_libraries(tst_Stats_Repository PRIVATE TrainingAppCore Qt6::Test)
add_test(NAME stats_repository COMMAND tst_Stats_Repository)
set_tests_properties(stats_repository PROPERTIES TIMEOUT 30)

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

qt_add_executable(tst_Auth_Api
    cpp/tst_Auth_Api.cpp
)
target_link_libraries(tst_Auth_Api PRIVATE TrainingAppCore TrainingAppTestSupport Qt6::Test)
add_test(NAME auth_api COMMAND tst_Auth_Api)
set_tests_properties(auth_api PROPERTIES TIMEOUT 20)

qt_add_executable(tst_Auth_Repository
    cpp/tst_Auth_Repository.cpp
)
target_link_libraries(tst_Auth_Repository PRIVATE TrainingAppCore TrainingAppTestSupport Qt6::Test)
add_test(NAME auth_repository COMMAND tst_Auth_Repository)
set_tests_properties(auth_repository PROPERTIES TIMEOUT 20)

qt_add_executable(tst_Avm_Auth
    cpp/tst_Avm_Auth.cpp
)
target_link_libraries(tst_Avm_Auth PRIVATE TrainingAppCore TrainingAppTestSupport Qt6::Test)
add_test(NAME auth_vm COMMAND tst_Avm_Auth)
set_tests_properties(auth_vm PROPERTIES TIMEOUT 30)

qt_add_executable(tst_Exercises_Api
    cpp/tst_Exercises_Api.cpp
)
target_link_libraries(tst_Exercises_Api PRIVATE TrainingAppCore TrainingAppTestSupport Qt6::Test)
add_test(NAME exercises_api COMMAND tst_Exercises_Api)
set_tests_properties(exercises_api PROPERTIES
    TIMEOUT 30
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:Qt6::Core>;$ENV{PATH}"
)

qt_add_executable(tst_Users_Api
    cpp/tst_Users_Api.cpp
)
target_link_libraries(tst_Users_Api PRIVATE TrainingAppCore TrainingAppTestSupport Qt6::Test)
add_test(NAME users_api COMMAND tst_Users_Api)
set_tests_properties(users_api PROPERTIES TIMEOUT 20)

qt_add_executable(tst_Users_Repository
    cpp/tst_Users_Repository.cpp
)
target_link_libraries(tst_Users_Repository PRIVATE TrainingAppCore TrainingAppTestSupport Qt6::Test)
add_test(NAME users_repository COMMAND tst_Users_Repository)
set_tests_properties(users_repository PROPERTIES TIMEOUT 20)

qt_add_executable(tst_Auth_Qml_Bridge
    cpp/tst_Auth_Qml_Bridge.cpp
)
target_link_libraries(tst_Auth_Qml_Bridge PRIVATE TrainingAppCore TrainingAppTestSupport Qt6::Test)
add_test(NAME auth_qml_bridge COMMAND tst_Auth_Qml_Bridge)
set_tests_properties(auth_qml_bridge PROPERTIES TIMEOUT 30)

qt_add_executable(tst_App_Logger
    cpp/tst_App_Logger.cpp
)
target_link_libraries(tst_App_Logger PRIVATE TrainingAppCore Qt6::Test)
add_test(NAME app_logger COMMAND tst_App_Logger)
set_tests_properties(app_logger PROPERTIES TIMEOUT 20)

add_library(TrainingAppTestSupport STATIC
    Support/Test_Http_Server.h
    Support/Test_Http_Server.cpp
)
target_include_directories(TrainingAppTestSupport PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(TrainingAppTestSupport PUBLIC Qt6::Core Qt6::Network)

qt_add_executable(tst_Program_Edit_VM cpp/tst_Program_Edit_VM.cpp)
target_link_libraries(tst_Program_Edit_VM PRIVATE TrainingAppCore Qt6::Test)
add_test(NAME program_edit_vm COMMAND tst_Program_Edit_VM)

qt_add_executable(tst_Workout_Logger cpp/tst_Workout_Logger.cpp)
target_link_libraries(tst_Workout_Logger PRIVATE TrainingAppCore Qt6::Test)
add_test(NAME workout_logger COMMAND tst_Workout_Logger)

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

# Экраны глобальной программы живут вне вкладок, поэтому до их появления в
# --screen аудит вёрстки их не видел вовсе. Каждый экран проверяется на всех
# компоновках проекта, включая тёмную и обрамлённую.
# profile-settings — раздел «Параметры» профиля: в нём блок «Оформление», и без
# именованного экрана он не попадал бы в аудит вёрстки ни на одной компоновке.
foreach(program_screen IN ITEMS program-catalog program-overview program-edit program-rules program-rules-picker program-stats program-day macro-calculator set-entry profile-settings)
    foreach(program_viewport IN ITEMS 360 390 480 frame dark)
        set(program_test_name "ui_smoke_${program_screen}_${program_viewport}")
        add_test(
            NAME "${program_test_name}"
            COMMAND "${CMAKE_COMMAND}"
                "-DAPP_EXECUTABLE=$<TARGET_FILE:appTrainingAppClient>"
                "-DQT_RUNTIME_DIR=$<TARGET_FILE_DIR:Qt6::Core>"
                "-DSCREEN=${program_screen}"
                "-DVIEWPORT=${program_viewport}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_ui_smoke.cmake"
        )
        set_tests_properties("${program_test_name}" PROPERTIES
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
