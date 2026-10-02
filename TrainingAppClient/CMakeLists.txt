cmake_minimum_required(VERSION 3.16)

project(TrainingAppClient VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(TRAINING_APP_GENERATE_QMLLS_INI "Generate .qmlls.ini for IDE builds" ON)
set(QT_QML_GENERATE_QMLLS_INI ${TRAINING_APP_GENERATE_QMLLS_INI})

find_package(Qt6 REQUIRED COMPONENTS Network Quick QuickControls2 QuickLayouts QuickEffects Sql)

qt_standard_project_setup(REQUIRES 6.8)
include(CTest)

# Content of downloaded design from Figma to Qt or similar
if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/importedcontent/CMakeLists.txt")
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/importedcontent)
endif()

qt_add_executable(appTrainingAppClient
    main.cpp
)

# Минимальный C++ core используется приложением и unit-тестами. Data/Domain/
# Presentation наполняются в следующих issues, зависимости Network/Sql уже
# фиксируют границу инфраструктурного слоя.
add_library(TrainingAppCore STATIC
    src/Presentation/List_Model.h
    src/Presentation/List_Model.cpp
    src/Presentation/Program_Rows.h
    src/Presentation/Program_Rows.cpp
    src/Presentation/Program_Stats_VM.h
    src/Presentation/Program_Stats_VM.cpp
    src/Presentation/Program_VM.h
    src/Presentation/Program_VM.cpp
    src/Presentation/Program_Edit_VM.h
    src/Presentation/Program_Edit_VM.cpp
    src/Presentation/Program_Rules_VM.h
    src/Presentation/Program_Rules_VM.cpp
    src/Presentation/Program_Catalog_VM.h
    src/Presentation/Program_Catalog_VM.cpp
    src/App/App_Context.h
    src/App/App_Context.cpp
    src/App/App_Logger.h
    src/App/App_Logger.cpp
    src/Domain/Error_State.h
    src/Domain/Error_State.cpp
    src/Domain/Program_Model.h
    src/Domain/Program_Model.cpp
    src/Domain/Program_Service.h
    src/Domain/Program_Service.cpp
    src/Domain/Rule_Engine.h
    src/Domain/Rule_Engine.cpp
    src/Domain/Workout_Logger.h
    src/Domain/Workout_Logger.cpp
    src/Domain/Progress_Stats.h
    src/Domain/Progress_Calculator.h
    src/Domain/Progress_Support.h
    src/Domain/Lifetime_Calculator.h
    src/Domain/Stats_Encoder.h
    src/Domain/Program_Stats_Calculator.h
    src/Domain/Progress_Calculator.cpp
    src/Domain/Progress_Support.cpp
    src/Domain/Lifetime_Calculator.cpp
    src/Domain/Stats_Encoder.cpp
    src/Domain/Program_Stats_Calculator.cpp
    src/Data/Program/Program_Repository_Base.h
    src/Data/Program/Program_Repository_Base.cpp
    src/Data/Program/Workout_Repository.h
    src/Data/Program/Workout_Repository.cpp
    src/Data/Program/Program_Repository.h
    src/Data/Program/Program_Repository.cpp
    src/Data/Program/Rule_Repository.h
    src/Data/Program/Rule_Repository.cpp
    src/Data/Program/Measurement_Repository.h
    src/Data/Program/Measurement_Repository.cpp
    src/Data/Program/Stats_Repository.h
    src/Data/Program/Stats_Repository.cpp
    src/Data/Auth/Auth_Api.h
    src/Data/Auth/Auth_Api.cpp
    src/Data/Auth/Auth_Repository.h
    src/Data/Auth/Auth_Repository.cpp
    src/Data/Auth/Auth_Session.h
    src/Data/Auth/Auth_Session.cpp
    src/Data/Auth/Token_Store.h
    src/Data/Auth/Token_Store.cpp
    src/Data/Database/Sql_Database.h
    src/Data/Database/Sql_Database.cpp
    src/Data/Network/Api_Error.h
    src/Data/Network/Api_Error.cpp
    src/Data/Network/Http_Client.h
    src/Data/Network/Http_Client.cpp
    src/Data/Users/Users_Api.h
    src/Data/Users/Users_Api.cpp
    src/Data/Users/Users_Repository.h
    src/Data/Users/Users_Repository.cpp
    src/Presentation/Auth_VM.h
    src/Presentation/Auth_VM.cpp
    src/Presentation/Auth_Qml_Bridge.h
    src/Presentation/Auth_Qml_Bridge.cpp
)
target_include_directories(TrainingAppCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(TrainingAppCore PUBLIC Qt6::Core Qt6::Network Qt6::Sql)

# Шрифт Inter (§3.2 дизайн-дока). Встраиваем только начертания, которые реально
# использует Typography.qml: Normal (Regular), Medium, DemiBold (Semi Bold),
# Bold. Курсивы и крайние веса (Thin/ExtraLight/ExtraBold/Black) в UI не
# применяются, поэтому в бинарник не попадают. LICENSE.txt (OFL) кладём рядом
# с шрифтами — лицензия требует поставлять её вместе со шрифтом.
qt_add_resources(appTrainingAppClient "inter_fonts"
    PREFIX "/fonts"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources/InterFont"
    FILES
        resources/InterFont/Inter-Regular.otf
        resources/InterFont/Inter-Medium.otf
        resources/InterFont/Inter-SemiBold.otf
        resources/InterFont/Inter-Bold.otf
        resources/InterFont/LICENSE.txt
)

# Design tokens and the demo mock layer are QML singletons (`pragma Singleton` inside the files).
# The build system does not detect the pragma on its own, so the source property is set explicitly.
set_source_files_properties(
    qml/Theme/Theme.qml
    qml/Theme/Typography.qml
    qml/Theme/Spacing.qml
    qml/Mock/Demo.qml
    qml/Mock/ApiErrorText.qml
    qml/Mock/MockApi.qml
    qml/Mock/MockCatalog.qml
    qml/Mock/MockSession.qml
    qml/Mock/MockTokenStore.qml
    qml/Presentation/AuthViewModel.qml
    qml/Presentation/ExercisesViewModel.qml
    qml/Presentation/PlansViewModel.qml
    qml/Presentation/ProfileViewModel.qml
    qml/Presentation/TodayViewModel.qml
    qml/Components/BackRouter.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(appTrainingAppClient
    URI TrainingAppClient
    SOURCES
        src/Platform/Platform_Chrome.h
        src/Platform/Platform_Chrome.cpp
        src/Presentation/Program_Qml.h
        src/Presentation/Program_Qml.cpp
    QML_FILES
        qml/Pages/Program/ProgramCatalogPage.qml
    qml/Pages/Program/ProgramOverviewPage.qml
        qml/Pages/Program/ProgramEditPage.qml
        qml/Pages/Program/ProgramDayPage.qml
        qml/Pages/Program/ProgramRulesPage.qml
        qml/Pages/Program/ProgramStatsPage.qml
        qml/Components/CheckinStatePill.qml
        qml/Components/BackBar.qml
        qml/Components/DateField.qml
        qml/Components/AppComboBox.qml
    qml/Components/AppNumberField.qml
    qml/Components/CatalogProgramRow.qml
    qml/Components/DayMapCell.qml
        qml/Main.qml
        qml/AppShell.qml
        qml/DevPanel.qml

        # Design tokens (singletons)
        qml/Theme/Theme.qml
        qml/Theme/Typography.qml
        qml/Theme/Spacing.qml

        # Presentation-only mock layer of the UI demo
        qml/Mock/Demo.qml
        qml/Mock/ApiErrorText.qml
        qml/Mock/MockApi.qml
        qml/Mock/MockCatalog.qml
        qml/Mock/MockSession.qml
        qml/Mock/MockTokenStore.qml

        # Stable presentation contracts (mock-backed until phase V)
        qml/Presentation/AuthFormController.qml
        qml/Presentation/AuthViewModel.qml
        qml/Presentation/ExercisesViewModel.qml
        qml/Presentation/PlansViewModel.qml
        qml/Presentation/ProfileViewModel.qml
        qml/Presentation/TodayViewModel.qml

        # UI components
        qml/Components/AppButton.qml
        qml/Components/AppCard.qml
        qml/Components/AppIcon.qml
        qml/Components/AppTextField.qml
        qml/Components/BackHandler.qml
        qml/Components/BackRouter.qml
        qml/Components/BottomTabBar.qml
        qml/Components/CatalogExerciseRow.qml
        qml/Components/ExerciseRow.qml
        qml/Components/FilterChips.qml
        qml/Components/FormChoice.qml
        qml/Components/HeroCard.qml
        qml/Components/LineChartCard.qml
        qml/Components/MetricCard.qml
        qml/Components/PageScaffold.qml
        qml/Components/RatingBadge.qml
        qml/Components/RecordRow.qml
        qml/Components/RegularityHeatmap.qml
        qml/Components/SegmentedControl.qml
        qml/Components/TagPill.qml

        # Screens
        qml/Pages/Auth/AuthPage.qml
        qml/Pages/Today/TodayPage.qml
        qml/Pages/Plans/PlansPage.qml
        qml/Pages/Exercises/ExercisesPage.qml
        qml/Pages/Profile/ProfilePage.qml
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appTrainingAppClient PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appTrainingAppClient
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# Каталог C++-кода (§0 docs/CODE_STYLE.md: код живёт в src/). Путь добавляем в
# include-путь, потому что сгенерированная qt_add_qml_module регистрация типов
# подключает заголовки по базовому имени: #if __has_include(<Platform_Chrome.h>).
target_include_directories(appTrainingAppClient PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/Platform)

# Те же правила для Presentation: сгенерированная регистрация типов ищет
# заголовки по базовому имени (#if __has_include(<Program_Qml.h>)).
target_include_directories(appTrainingAppClient PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/Presentation)

target_link_libraries(appTrainingAppClient
    PRIVATE TrainingAppCore Qt6::Quick Qt6::QuickControls2 Qt6::QuickLayouts Qt6::QuickEffects
)

# Собственный пакет Android (portrait-lock, имя приложения): этап M1.
# Свойство учитывается только Android-сборками, на десктопе игнорируется.
set_target_properties(appTrainingAppClient PROPERTIES
    QT_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/android
)

install(TARGETS appTrainingAppClient
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# QML unit/smoke tests run on desktop. Android tests need a deployed test
# runner and are outside the local CTest contour.
if (BUILD_TESTING AND NOT ANDROID)
    find_package(Qt6 REQUIRED COMPONENTS QuickTest Test)
    add_subdirectory(tests)
endif()
