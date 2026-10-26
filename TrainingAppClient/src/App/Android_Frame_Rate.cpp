#include "App/Android_Frame_Rate.h"

#include <QLoggingCategory>

#ifdef Q_OS_ANDROID
#include <QJniEnvironment>
#include <QJniObject>
#include <QtCore/qcoreapplication_platform.h>
#endif

Q_LOGGING_CATEGORY(ANDROID_FRAME_RATE_Log, "trainingapp.android.framerate")

#ifdef Q_OS_ANDROID
//-------------------------------------------------------------------------------------------------------------
// Самый частотный режим дисплея. Аргументы: display — android.view.Display;
// best_rate получает частоту найденного режима. Возврат: id режима
// (preferredDisplayModeId), 0 — режимы определить не удалось.
//-------------------------------------------------------------------------------------------------------------
static int Best_Display_Mode(const QJniObject &display, float *best_rate)
{
	QJniEnvironment environment;
	QJniObject modes;
	QJniObject mode;
	jobjectArray mode_array;
	jsize count;
	jsize i;
	float rate;
	int best_id;
	int mode_id;

	*best_rate = 0.0f;
	best_id = 0;

	if (!display.isValid())
		return 0;

	modes = display.callObjectMethod("getSupportedModes", "()[Landroid/view/Display$Mode;");

	if (!modes.isValid())
		return 0;

	mode_array = (jobjectArray)modes.object();
	count = environment->GetArrayLength(mode_array);

	for (i = 0; i < count; ++i)
	{
		mode = QJniObject::fromLocalRef(environment->GetObjectArrayElement(mode_array, i));

		if (!mode.isValid())
			continue;

		rate = mode.callMethod<jfloat>("getRefreshRate", "()F");
		mode_id = mode.callMethod<jint>("getModeId", "()I");

		if (rate > *best_rate)
		{
			*best_rate = rate;
			best_id = mode_id;
		}
	}

	return best_id;
}
//-------------------------------------------------------------------------------------------------------------
bool AsAndroid_Frame_Rate::Request_Highest()
{
	QJniObject activity = QNativeInterface::QAndroidApplication::context();
	QJniObject window;
	QJniObject window_manager;
	QJniObject display;
	QJniObject attributes;
	float best_rate;
	int best_mode;

	if (!activity.isValid())
	{
		qCWarning(ANDROID_FRAME_RATE_Log) << "android activity is not available";

		return false;
	}

	window = activity.callObjectMethod("getWindow", "()Landroid/view/Window;");

	if (!window.isValid())
	{
		qCWarning(ANDROID_FRAME_RATE_Log) << "android window is not available";

		return false;
	}

	window_manager = window.callObjectMethod("getWindowManager", "()Landroid/view/WindowManager;");

	if (window_manager.isValid())
		display = window_manager.callObjectMethod("getDefaultDisplay", "()Landroid/view/Display;");

	best_mode = Best_Display_Mode(display, &best_rate);
	attributes = window.callObjectMethod("getAttributes", "()Landroid/view/WindowManager$LayoutParams;");

	if (!attributes.isValid())
	{
		qCWarning(ANDROID_FRAME_RATE_Log) << "window attributes are not available";

		return false;
	}

	if (best_mode > 0)
	{
		// Точный режим надёжнее частоты: preferredDisplayModeId переключает
		// окно в конкретный режим, тогда как абстрактные 120 Гц система вправе
		// игнорировать, если панель, например, 90-герцевая.
		attributes.setField<jint>("preferredDisplayModeId", (jint)best_mode);
		attributes.setField<jfloat>("preferredRefreshRate", best_rate);
		qCInfo(ANDROID_FRAME_RATE_Log) << "requested display mode" << best_mode << "at" << best_rate << "Hz";
	}
	else
	{
		// Режимы определить не удалось (нестандартная прошивка) — просим
		// частоту, как раньше: 120 Гц, система выберет ближайшую поддержку.
		attributes.setField<jfloat>("preferredRefreshRate", 120.0f);
		qCInfo(ANDROID_FRAME_RATE_Log) << "requested 120 Hz display rate";
	}

	window.callMethod<void>("setAttributes", "(Landroid/view/WindowManager$LayoutParams;)V",
	                        attributes.object<jobject>());

	return true;
}
#else
bool AsAndroid_Frame_Rate::Request_Highest()
{
	// Не Android: частота кадров задаётся дисплеем и оконной системой, просить
	// нечего. Вызов остаётся безопасным для запуска на десктопе и в CI.
	return false;
}
#endif
//-------------------------------------------------------------------------------------------------------------
