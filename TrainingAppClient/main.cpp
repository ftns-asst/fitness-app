#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

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
	QQmlApplicationEngine engine;
	int loaded_fonts;

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

	engine.loadFromModule("TrainingAppClient", "Main");

	return QGuiApplication::exec();
}
//---------------------------------------------------------------------------
