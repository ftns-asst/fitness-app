#include "App/Android_Frame_Rate.h"
#include "App/App_Context.h"

#include "App/App_Logger.h"
#include "Data/Exercises/Exercises_Repository.h"
#include "Presentation/Auth_Qml_Bridge.h"
#include "Presentation/Exercises_Editor_VM.h"
#include "Presentation/Exercises_One_VM.h"
#include "Presentation/Exercises_VM.h"
#include "Presentation/Macro_VM.h"
#include "Presentation/Program_Catalog_VM.h"
#include "Presentation/Program_Edit_VM.h"
#include "Presentation/Program_Rules_VM.h"
#include "Presentation/Program_Stats_VM.h"
#include "Presentation/Program_VM.h"
#include "Presentation/Today_VM.h"

#include <QCoreApplication>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QVariant>

//---------------------------------------------------------------------------
// Регистрация начертаний Inter из ресурсов (§3.2 дизайн-дока). Загружаем до
// создания QML-движка, чтобы первый кадр рисовался Inter, а не системным
// шрифтом. Аргументы: нет. Возврат: число загруженных начертаний (ожидаем 4;
// меньше — ресурс не собран, UI остаётся рабочим на системном шрифте).
//---------------------------------------------------------------------------
static int Load_Inter_Fonts()
{
	QStringList font_files;
	int loaded;
	int i;

	font_files << ":/fonts/Inter-Regular.otf" << ":/fonts/Inter-Medium.otf" << ":/fonts/Inter-SemiBold.otf"
	           << ":/fonts/Inter-Bold.otf";
	loaded = 0;

	for (i = 0; i < font_files.size(); ++i)
	{
		// Отрицательный id — файл не найден или не является шрифтом.
		if (QFontDatabase::addApplicationFont(font_files.at(i)) >= 0)
		{
			++loaded;
		}
	}

	return loaded;
}
//---------------------------------------------------------------------------
int main(int argc, char *argv[])
{
	// Локальные объекты объявлены до исполняемого кода (§5.2).
	QGuiApplication app(argc, argv);
	AsApp_Logger logger;
	AsApp_Context app_context;
	QQmlApplicationEngine engine;
	QStringList arguments;
	QString log_directory;
	int loaded_fonts;
	bool context_initialized;
	bool logger_opened;
	bool real_auth;

	QCoreApplication::setOrganizationName("FitnessAssistant");
	QCoreApplication::setOrganizationDomain("fitness.nought.ru");
	QCoreApplication::setApplicationName("TrainingAppClient");

	// Логгер ставится до инициализации контекста: строки инициализации
	// попадают и в stderr (консоль/CI), и в приватный logs/app.log.
	log_directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs";
	logger_opened = logger.Open(log_directory);

	if (logger_opened == false)
		qWarning("Logger: файл лога недоступен, продолжаем только в stderr");

	qInstallMessageHandler(&AsApp_Logger::Handle_Message);

	arguments = QCoreApplication::arguments();
	real_auth = arguments.indexOf("--real-auth") >= 0;

	// Высокая частота кадров (Android): максимальный режим дисплея запрашивается
	// до загрузки QML и до создания окна — режим применяется при показе
	// поверхности, и запрос после первого кадра уже опоздал бы. Раньше просьба
	// была за флагом --high-refresh, но 60 Гц на 90/120-герцевом телефоне — это
	// дёрганый скролл и анимации, из-за которых приложение и оставляют.
	// На других ОС вызов no-op.
	AsAndroid_Frame_Rate::Request_Highest();

	qInfo("app started: qt=%s real_auth=%d", qVersion(), real_auth ? 1 : 0);

	context_initialized = app_context.Initialize(arguments);

	if (context_initialized == false)
	{
		qCritical("App context initialization failed: %s", qUtf8Printable(app_context.Last_Error()));
		return 2;
	}

	loaded_fonts = Load_Inter_Fonts();

	if (loaded_fonts < 4)
	{
		// Инвариант: в ресурсах должны быть все четыре начертания (§5.6).
		qWarning("Inter: загружено начертаний %d из 4", loaded_fonts);
	}

	// Нейтральный базовый стиль: весь визуал строится на собственных токенах
	// (см. qml/Theme/Theme.qml). Важно вызвать до загрузки QML, где создаются
	// контролы Qt Quick Controls.
	QQuickStyle::setStyle("Basic");

	QObject::connect(
	    &engine, &QQmlApplicationEngine::objectCreationFailed, &app, []() { QCoreApplication::exit(-1); },
	    Qt::QueuedConnection);

	// --real-auth: экран входа работает с настоящим backend через Avm_Auth_Bridge
	// (контекст-свойство AuthBackend). Без флага UI остаётся на presentation-mock
	// (qmltestrunner свойства не видит, все QML-тесты остаются на mock).
	if (real_auth)
		engine.rootContext()->setContextProperty("AuthBackend", app_context.Auth_Bridge());
	else
		engine.rootContext()->setContextProperty("AuthBackend", QVariant());

	engine.rootContext()->setContextProperty("AppContext", &app_context);

	// Стек модели программы (срез W11): экраны «Моя программа», «Правила» и
	// «Статистика» работают на реальных данных из SQLite. Экземпляры создаёт
	// composition root, поэтому экраны не знают о репозиториях.
	//
	// Передаются как QObject*: контекстное свойство QML хранится в QVariant,
	// а пользовательские указатели на QObject конвертируются всегда, тогда как
	// типы, не объявленные метай-типами, — нет.
	engine.rootContext()->setContextProperty("ProgramVM", static_cast<QObject *>(app_context.Program_VM()));
	engine.rootContext()->setContextProperty("ProgramCatalogVM",
	                                         static_cast<QObject *>(app_context.Program_Catalog_VM()));
	engine.rootContext()->setContextProperty("ProgramStatsVM", static_cast<QObject *>(app_context.Program_Stats_VM()));
	engine.rootContext()->setContextProperty("ProgramRulesVM", static_cast<QObject *>(app_context.Program_Rules_VM()));
	engine.rootContext()->setContextProperty("ProgramEditVM", static_cast<QObject *>(app_context.Program_Edit_VM()));
	// Главный экран «Сегодня» на данных (issue 27): C++-слой готов, разметка
	// TodayPage переводится на него отдельным шагом.
	engine.rootContext()->setContextProperty("TodayVM", static_cast<QObject *>(app_context.Today_VM()));
	// Экран «Рассчитать» КБЖУ (спринт S7): модель считает по формулам домена и
	// хранит дату рождения и активность в profile_local (схема v5).
	engine.rootContext()->setContextProperty("MacroVM", static_cast<QObject *>(app_context.Macro_VM()));
	// Каталог упражнений на живом API (спринт U4, issue 30).
	engine.rootContext()->setContextProperty("ExercisesVM", static_cast<QObject *>(app_context.Exercises_VM()));
	// Форма своего упражнения (спринт U5): создание, правка, удаление.
	engine.rootContext()->setContextProperty("ExercisesEditorVM",
	                                         static_cast<QObject *>(app_context.Exercises_Editor_VM()));
	// Детальная карточка упражнения (спринт U6).
	engine.rootContext()->setContextProperty("ExercisesOneVM", static_cast<QObject *>(app_context.Exercises_One_VM()));

	engine.loadFromModule("TrainingAppClient", "Main");

	return QGuiApplication::exec();
}
//---------------------------------------------------------------------------
