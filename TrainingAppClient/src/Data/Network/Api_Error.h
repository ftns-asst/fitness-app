#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QString>

//----------------------------------------------------------------------------
// Нормализованная ошибка API. Backend key имеет приоритет; при пустом или
// невалидном error DTO ключ выводится из HTTP/network состояния.
//----------------------------------------------------------------------------
class AApi_Error
{
public:
	AApi_Error();
	AApi_Error(const QString &in_key, const QString &in_message, int in_http_status_code, int in_network_error_code);

	QString Key() const;
	QString Message() const;
	int HTTP_Status_Code() const;
	int Network_Error_Code() const;
	bool Is_Error() const;

	static AApi_Error From_Response(int in_http_status_code, int in_network_error_code,
	                                const QString &in_network_error_text, const QByteArray &in_body);

private:
	static QString Fallback_Key(int in_http_status_code, int in_network_error_code);

	QString Error_Key;
	QString Error_Message;
	int HTTP_Status = 0;
	int Network_Error = 0;
};
//----------------------------------------------------------------------------

Q_DECLARE_METATYPE(AApi_Error)
