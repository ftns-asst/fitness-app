#include "Platform_Chrome.h"

#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <qcoreapplication_platform.h>
#endif

#ifdef Q_OS_ANDROID
//----------------------------------------------------------------------------
// View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR | View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR.
//----------------------------------------------------------------------------
static const jint Light_System_Bars = 8192 | 16;
//----------------------------------------------------------------------------
// Устанавливает appearance системных полос: in_dark_mode = true — светлые
// иконки, false — тёмные. Возврат: нет; при невалидных JNI-объектах выход
// без изменений (API < 30). Локальные переменные объявлены до кода (§5.2).
//----------------------------------------------------------------------------
static void Set_System_Bars_Appearance(bool in_dark_mode)
{
	QJniObject activity;
	QJniObject window;
	QJniObject controller;
	jint appearance;

	// Публичный JNI-путь до activity Qt-приложения.
	activity =
	    QJniObject::callStaticObjectMethod("org/qtproject/qt/android/QtNative", "activity", "()Landroid/app/Activity;");

	if (activity.isValid() == false)
	{
		return;
	}

	window = activity.callObjectMethod("getWindow", "()Landroid/view/Window;");

	if (window.isValid() == false)
	{
		return;
	}

	controller = window.callObjectMethod("getWindowInsetsController", "()Landroid/view/WindowInsetsController;");

	if (controller.isValid() == false)
	{
		// API < 30: оставляем системные значения по умолчанию.
		return;
	}

	// Тёмная тема — светлые иконки (appearance = 0), светлая тема — тёмные.
	appearance = 0;

	if (in_dark_mode == false)
	{
		appearance = Light_System_Bars;
	}

	controller.callMethod<void>("setSystemBarsAppearance", "(II)V", appearance, Light_System_Bars);
}
#endif

//----------------------------------------------------------------------------
// APlatform_Chrome
//----------------------------------------------------------------------------
APlatform_Chrome::APlatform_Chrome(QObject *in_parent) : QObject(in_parent)
{
}
//----------------------------------------------------------------------------
void APlatform_Chrome::applyChrome(bool in_dark_mode)
{
#ifdef Q_OS_ANDROID
	QNativeInterface::QAndroidApplication::runOnAndroidMainThread([in_dark_mode]
	                                                              { Set_System_Bars_Appearance(in_dark_mode); });
#else
	Q_UNUSED(in_dark_mode)
#endif
}
//----------------------------------------------------------------------------
