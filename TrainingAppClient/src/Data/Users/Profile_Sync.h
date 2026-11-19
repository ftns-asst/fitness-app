#pragma once

#include "Data/Network/Network_Monitor.h"
#include "Data/Users/Users_Repository.h"

#include <QObject>
#include <QTimer>

//-------------------------------------------------------------------------------------------------------------
// Доставка сохранённых параметров профиля текущей сессии. Остальные типы outbox не обрабатывает.
//-------------------------------------------------------------------------------------------------------------
class AProfile_Sync : public QObject
{
	Q_OBJECT

public:
	explicit AProfile_Sync(AUsers_Repository *in_repository, AsAuth_Session *in_session, AsNetwork_Monitor *in_network,
	                       QObject *in_parent = 0);
	bool Is_Syncing(const QString &in_user_id) const;
	void Retry(const QString &in_user_id);

signals:
	void changed(const QString &userId);

public slots:
	void Flush();

private slots:
	void on_Profile_Updated(int in_request_id, const SUsers_Profile &in_profile, const AApi_Error &in_error);
	void on_Session_Changed();
	void on_Connection_Restored();

private:
	AUsers_Repository *Repository_Instance;
	AsAuth_Session *Session_Instance;
	AsNetwork_Monitor *Network_Instance;
	QTimer Retry_Timer;
	QString In_Flight_User_ID;
	qint64 In_Flight_Queue_ID = 0;
	int Request_ID = 0;
};
//-------------------------------------------------------------------------------------------------------------
