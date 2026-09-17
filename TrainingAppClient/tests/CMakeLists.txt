# qmltestrunner is a separate process and cannot see resources embedded in the
# application executable. Build a test import directory from the exact
# production mock files instead of maintaining test copies.
set(MOCK_IMPORT_ROOT "${CMAKE_CURRENT_BINARY_DIR}/imports")
set(MOCK_IMPORT_DIR "${MOCK_IMPORT_ROOT}/TrainingAppClient")
file(MAKE_DIRECTORY "${MOCK_IMPORT_DIR}")

set(MOCK_QML_FILES
    ApiErrorText.qml
    MockApi.qml
    MockCatalog.qml
    MockSession.qml
    MockTokenStore.qml
)

foreach(mock_file IN LISTS MOCK_QML_FILES)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/../qml/Mock/${mock_file}"
        "${MOCK_IMPORT_DIR}/${mock_file}"
        COPYONLY
    )
endforeach()

file(WRITE "${MOCK_IMPORT_DIR}/qmldir"
    "module TrainingAppClient\n"
    "singleton ApiErrorText 1.0 ApiErrorText.qml\n"
    "singleton MockApi 1.0 MockApi.qml\n"
    "singleton MockCatalog 1.0 MockCatalog.qml\n"
    "singleton MockSession 1.0 MockSession.qml\n"
    "singleton MockTokenStore 1.0 MockTokenStore.qml\n"
)

if (TARGET Qt6::qmltestrunner)
    set(QML_TEST_RUNNER "$<TARGET_FILE:Qt6::qmltestrunner>")
else()
    find_program(QML_TEST_RUNNER qmltestrunner REQUIRED)
endif()

add_test(
    NAME qml_mock_contract
    COMMAND "${QML_TEST_RUNNER}"
        -input "${CMAKE_CURRENT_SOURCE_DIR}/qml"
        -import "${MOCK_IMPORT_ROOT}"
        -platform offscreen
)

set_tests_properties(qml_mock_contract PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QML_DISABLE_DISK_CACHE=1"
    TIMEOUT 30
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