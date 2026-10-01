#include "App/App_Logger.h"

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QMutexLocker>

#include <cstdio>

static AsApp_Logger *Active_Logger_Instance = 0;

//----------------------------------------------------------------------------
// AsApp_Logger
//----------------------------------------------------------------------------
AsApp_Logger::AsApp_Logger(QObject *in_parent) : QObject(in_parent)
{
}
//----------------------------------------------------------------------------
AsApp_Logger::~AsApp_Logger()
{
	Close();
}
//----------------------------------------------------------------------------
bool AsApp_Logger::Open(const QString &in_directory, int in_max_bytes)
{
	QDir directory(in_directory);

	Close();

	Max_Bytes = in_max_bytes > 0 ? in_max_bytes : (int)EDefault_Max_Bytes;

	// Каталог может создаваться параллельным процессом (урок issue 6).
	if (directory.exists() || directory.mkpath(".") || directory.exists())
	{
		Log_File.setFileName(directory.filePath("app.log"));
		Backup_File_Path = directory.filePath("app.1.log");

		if (Log_File.open(QIODevice::Append | QIODevice::Text))
		{
			Active_Logger_Instance = this;
			return true;
		}
	}

	return false;
}
//----------------------------------------------------------------------------
void AsApp_Logger::Close()
{
	if (Active_Logger_Instance == this)
		Active_Logger_Instance = 0;

	if (Log_File.isOpen())
		Log_File.close();

	Backup_File_Path.clear();
}
//----------------------------------------------------------------------------
void AsApp_Logger::Handle_Message(QtMsgType in_type, const QMessageLogContext &in_context, const QString &in_message)
{
	QString line;
	QByteArray utf8;
	AsApp_Logger *logger = Active_Logger_Instance;

	line = QString("%1 %2 %3: %4")
	           .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"))
	           .arg(Level_Name(in_type))
	           .arg(QString::fromLatin1(in_context.category))
	           .arg(in_message);
	utf8 = line.toUtf8();

	// stderr сохраняем всегда: консоль, CI и offscreen-прогоны читают его.
	fwrite(utf8.constData(), 1, utf8.size(), stderr);
	fwrite("\n", 1, 1, stderr);
	fflush(stderr);

	if (logger != 0)
		logger->Append(line);
}
//----------------------------------------------------------------------------
void AsApp_Logger::Append(const QString &in_line)
{
	QMutexLocker locker(&Write_Mutex);
	QByteArray utf8;

	if (Log_File.isOpen() == false)
		return;

	utf8 = in_line.toUtf8();
	utf8.append('\n');

	if (Log_File.size() + utf8.size() > Max_Bytes && Rotate() == false)
		return;

	Log_File.write(utf8);
	Log_File.flush();
}
//----------------------------------------------------------------------------
bool AsApp_Logger::Rotate()
{
	Log_File.close();
	QFile::remove(Backup_File_Path);

	// Логи изнутри логгера запрещены: сообщение прошло бы через Handle_Message
	// и могло рекурсивно вызвать ротацию. Ошибки молча игнорируем — лог
	// не является критичной функциональностью.
	QFile::rename(Log_File.fileName(), Backup_File_Path);

	return Log_File.open(QIODevice::Append | QIODevice::Text);
}
//----------------------------------------------------------------------------
QString AsApp_Logger::Level_Name(QtMsgType in_type)
{
	switch (in_type)
	{
	case QtDebugMsg:
		return "debug";
	case QtInfoMsg:
		return "info";
	case QtWarningMsg:
		return "warning";
	case QtCriticalMsg:
		return "critical";
	case QtFatalMsg:
		return "fatal";
	default:
		break;
	}

	return "unknown";
}
//----------------------------------------------------------------------------