#pragma once

#include <QHash>
#include <QObject>
#include <QTimer>
#include <QVariant>

class AsAuth_Session;
class AsNetwork_Monitor;
class AsSql_Database;
class AWorkout_Repository;
struct SHttp_Response;

//-------------------------------------------------------------------------------------------------------------
class AWorkout_Sync : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QString userId READ userId NOTIFY accountChanged)
	Q_PROPERTY(QVariantList history READ history NOTIFY changed)
	Q_PROPERTY(QVariantMap detail READ detail NOTIFY changed)
	Q_PROPERTY(bool loading READ loading NOTIFY changed)
	Q_PROPERTY(bool hasMore READ hasMore NOTIFY changed)
	Q_PROPERTY(QString errorKey READ errorKey NOTIFY changed)

public:
	AWorkout_Sync(AsSql_Database *in_database, AWorkout_Repository *in_workouts, AsAuth_Session *in_session,
	              AsNetwork_Monitor *in_network, QObject *in_parent = 0);

	QString userId() const;
	QVariantList history() const;
	QVariantMap detail() const;
	bool loading() const;
	bool hasMore() const;
	QString errorKey() const;

	Q_INVOKABLE void refreshHistory();
	Q_INVOKABLE void loadMoreHistory();
	Q_INVOKABLE void openWorkout(const QString &in_id);

public slots:
	void Flush();

signals:
	void changed();
	void accountChanged();

private:
	void Reset_Account();
	void Send_History(int in_offset);
	void Receive(int in_id, const SHttp_Response &in_response);
	void Finish_Send(const SHttp_Response &in_response);
	void Rebuild_History();
	void Load_Local_Detail(const QString &in_id);

	AsSql_Database *Database;
	AWorkout_Repository *Workouts;
	AsAuth_Session *Session;
	AsNetwork_Monitor *Network;
	QTimer Retry_Timer;
	QHash<QString, QVariantMap> Cloud;
	QVariantList History;
	QVariantMap Detail;
	QString Detail_Id;
	QString Error_Key;
	QString Owner;
	QString Sending_Id;
	qint64 Queue_Id = 0;
	int Attempts = 0;
	int Upload_Request = 0;
	int History_Request = 0;
	int Detail_Request = 0;
	int History_Offset = 0;
	int Pending_Offset = 0;
	bool Has_More = false;
};
//-------------------------------------------------------------------------------------------------------------
