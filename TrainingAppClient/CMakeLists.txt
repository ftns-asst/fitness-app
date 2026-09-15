cmake_minimum_required(VERSION 3.16)

project(TrainingAppClient VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(QT_QML_GENERATE_QMLLS_INI ON)

find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 QuickLayouts QuickEffects)

qt_standard_project_setup(REQUIRES 6.8)

# Content of downloaded design from Figma to Qt or similar
if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/importedcontent/CMakeLists.txt")
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/importedcontent)
endif()

qt_add_executable(appTrainingAppClient
    main.cpp
)

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
    qml/Mock/MockCatalog.qml
    qml/Components/BackRouter.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(appTrainingAppClient
    URI TrainingAppClient
    SOURCES
        src/Platform/Platform_Chrome.h
        src/Platform/Platform_Chrome.cpp
    QML_FILES
        qml/Main.qml
        qml/AppShell.qml
        qml/DevPanel.qml

        # Design tokens (singletons)
        qml/Theme/Theme.qml
        qml/Theme/Typography.qml
        qml/Theme/Spacing.qml

        # Presentation-only mock layer of the UI demo
        qml/Mock/Demo.qml
        qml/Mock/MockCatalog.qml

        # UI components
        qml/Components/AppButton.qml
        qml/Components/AppCard.qml
        qml/Components/AppIcon.qml
        qml/Components/BackHandler.qml
        qml/Components/BackRouter.qml
        qml/Components/BottomTabBar.qml
        qml/Components/CatalogExerciseRow.qml
        qml/Components/ExerciseRow.qml
        qml/Components/FilterChips.qml
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

target_link_libraries(appTrainingAppClient
    PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::QuickLayouts Qt6::QuickEffects
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
