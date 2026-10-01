#include "App/App_Logger.h"

#include <QFile>
#include <QLoggingCategory>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest>

Q_LOGGING_CATEGORY(Logger_Test_Log, "trainingapp.logger.test")

class AApp_Logger_Test : public QObject
{
	Q_OBJECT

private slots:
	void writesMessagesToFileWithCategoryAndLevel();
	void rotationMovesFullLogToBackup();
	void handlerRestoreStopsFileWriting();

private:
	static QString Read_Whole_File(const QString &in_path);
	static QString Log_Path(const QTemporaryDir &in_directory);
	static QString Backup_Path(const QTemporaryDir &in_directory);
};

//----------------------------------------------------------------------------
void AApp_Logger_Test::writesMessagesToFileWithCategoryAndLevel()
{
	QTemporaryDir directory;
	AsApp_Logger logger;
	QString content;

	QVERIFY(directory.isValid());
	QVERIFY(logger.Open(directory.filePath("logs")));

	qInstallMessageHandler(&AsApp_Logger::Handle_Message);
	qCInfo(Logger_Test_Log).noquote() << "logger smoke info";
	qCWarning(Logger_Test_Log).noquote() << "logger smoke warning";
	qInstallMessageHandler(0);

	content = Read_Whole_File(Log_Path(directory));
	QVERIFY(content.contains("logger smoke info"));
	QVERIFY(content.contains("logger smoke warning"));
	QVERIFY(content.contains("trainingapp.logger.test"));
	QVERIFY(content.contains("info"));
	QVERIFY(content.contains("warning"));

	logger.Close();
}
//----------------------------------------------------------------------------
void AApp_Logger_Test::rotationMovesFullLogToBackup()
{
	QTemporaryDir directory;
	AsApp_Logger logger;
	QString current;
	QString backup;
	int i;

	QVERIFY(directory.isValid());
	QVERIFY(logger.Open(directory.filePath("logs"), 300));

	qInstallMessageHandler(&AsApp_Logger::Handle_Message);

	// Малый лимит: несколько строк гарантированно провоцируют ротацию.
	for (i = 0; i < 12; ++i)
		qCInfo(Logger_Test_Log).noquote() << "rotation line" << i;

	qInstallMessageHandler(0);

	current = Read_Whole_File(Log_Path(directory));
	backup = Read_Whole_File(Backup_Path(directory));
	QVERIFY(current.contains("rotation line 11"));
	QVERIFY(current.contains("rotation line 0") == false);
	QVERIFY(backup.isEmpty() == false);
	QVERIFY(backup.contains("rotation line"));
	QVERIFY(backup.contains("rotation line 11") == false);

	logger.Close();
}
//----------------------------------------------------------------------------
void AApp_Logger_Test::handlerRestoreStopsFileWriting()
{
	QTemporaryDir directory;
	AsApp_Logger logger;
	QString content;

	QVERIFY(directory.isValid());
	QVERIFY(logger.Open(directory.filePath("logs")));

	qInstallMessageHandler(&AsApp_Logger::Handle_Message);
	qCInfo(Logger_Test_Log).noquote() << "before restore";
	qInstallMessageHandler(0);
	qCInfo(Logger_Test_Log).noquote() << "after restore";

	content = Read_Whole_File(Log_Path(directory));
	QVERIFY(content.contains("before restore"));
	QVERIFY(content.contains("after restore") == false);

	logger.Close();
}
//----------------------------------------------------------------------------
QString AApp_Logger_Test::Read_Whole_File(const QString &in_path)
{
	QFile file(in_path);

	if (file.open(QIODevice::ReadOnly | QIODevice::Text) == false)
		return QString();

	QString content = QString::fromUtf8(file.readAll());
	file.close();

	return content;
}
//----------------------------------------------------------------------------
QString AApp_Logger_Test::Log_Path(const QTemporaryDir &in_directory)
{
	return in_directory.filePath("logs/app.log");
}
//----------------------------------------------------------------------------
QString AApp_Logger_Test::Backup_Path(const QTemporaryDir &in_directory)
{
	return in_directory.filePath("logs/app.1.log");
}
//----------------------------------------------------------------------------

QTEST_MAIN(AApp_Logger_Test)

#include "tst_App_Logger.moc"