#pragma once

#include <QByteArray>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>

//----------------------------------------------------------------------------
// Корневой контекст приложения. На этапе issue 3 владеет только конфигурацией
// окружения; сетевые, SQL- и presentation-сервисы добавляются последующими
// issues и создаются здесь в порядке их зависимостей.
//----------------------------------------------------------------------------
class AsApp_Context : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QString apiBaseUrl READ apiBaseUrl CONSTANT)
	Q_PROPERTY(QString apiUrlSource READ apiUrlSource CONSTANT)
	Q_PROPERTY(bool initialized READ initialized CONSTANT)

public:
	explicit AsApp_Context(QObject *in_parent = 0);

	bool Initialize(const QStringList &in_arguments);
	bool Initialize(const QStringList &in_arguments, QSettings &in_settings, const QByteArray &in_environment_url);
	QString apiBaseUrl() const;
	QString apiUrlSource() const;
	bool initialized() const;
	QString Last_Error() const;

	static QString Default_Api_Base_URL();
	static QString Environment_Variable_Name();
	static QString Settings_Key();

private:
	static bool Normalize_Api_Base_URL(const QString &in_value, QString &out_value, QString &out_error);
	static bool Read_Command_Line_URL(const QStringList &in_arguments, QString &out_value, bool &out_found, QString &out_error);

	QString Api_Base_URL;
	QString Api_URL_Source;
	bool Initialized = false;
	QString Last_Error_Text;
};
//----------------------------------------------------------------------------