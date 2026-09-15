#pragma once

#include <QObject>
#include <QQmlEngine>

//----------------------------------------------------------------------------
// Синхронизация системных полос Android (status bar, navigation bar) с темой
// приложения: в светлой теме системные иконки тёмные, в тёмной — светлые.
// На не-Android платформах — no-op. Реализация: WindowInsetsController (API 30+),
// на более старых API молча ничего не делаем.
// Цвет самих полос не задаём: Qt 6.9+ на актуальном targetSdk работает
// edge-to-edge, отступы учтены через SafeArea в qml/Main.qml.
// Имя класса — по код-стайлу (§2: префикс A, слова через «_»); в QML тип
// экспортируется под прежним именем PlatformChrome через QML_NAMED_ELEMENT.
//----------------------------------------------------------------------------
class APlatform_Chrome : public QObject
{
	Q_OBJECT
	QML_NAMED_ELEMENT(PlatformChrome)
	QML_SINGLETON

public:
	// in_parent — родитель QObject (владелец); время жизни — механизм Qt.
	explicit APlatform_Chrome(QObject *in_parent = 0);

	// Перекрашивает иконки системных полос под тему: in_dark_mode = true —
	// светлые иконки, false — тёмные. Возврат: нет; вне Android — no-op.
	Q_INVOKABLE void applyChrome(bool in_dark_mode);
};
//----------------------------------------------------------------------------
