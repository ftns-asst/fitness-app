#pragma once

#include <QFile>
#include <QMutex>
#include <QObject>
#include <QString>

class QMessageLogContext;

//----------------------------------------------------------------------------
// Файловый логгер приложения. Ставится как Qt message handler в main.cpp до
// инициализации AsApp_Context: каждая строка пишется и в stderr (диагностика
// консоли и CI), и в logs/app.log приватного каталога с ротацией по размеру
// (app.1.log — одна резервная копия). Категории trainingapp.* управляются
// стандартным QT_LOGGING_RULES. Инвариант приватности (docs/01, §6): в логи
// не попадают email, пароли, токены, query и тела запросов.
//----------------------------------------------------------------------------
class AsApp_Logger : public QObject
{
	Q_OBJECT

public:
	enum ELog_Limits
	{
		EDefault_Max_Bytes = 2 * 1024 * 1024
	};

	explicit AsApp_Logger(QObject *in_parent = 0);
	~AsApp_Logger() override;

	bool Open(const QString &in_directory, int in_max_bytes = EDefault_Max_Bytes);
	void Close();
	static void Handle_Message(QtMsgType in_type, const QMessageLogContext &in_context, const QString &in_message);

private:
	void Append(const QString &in_line);
	bool Rotate();
	static QString Level_Name(QtMsgType in_type);

	QFile Log_File;
	QMutex Write_Mutex;
	QString Backup_File_Path;
	int Max_Bytes = EDefault_Max_Bytes;
};
//----------------------------------------------------------------------------