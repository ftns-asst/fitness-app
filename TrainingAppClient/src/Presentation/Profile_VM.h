#pragma once

#include "Data/Network/Network_Monitor.h"
#include "Data/Users/Profile_Sync.h"
#include "Data/Users/Users_Repository.h"

#include <QObject>
#include <QVariantMap>

//-------------------------------------------------------------------------------------------------------------
// Профиль текущей учётной записи: локальное сохранение и состояние отложенной отправки.
//-------------------------------------------------------------------------------------------------------------
class Avm_Profile : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QVariantMap profile READ profile NOTIFY changed)
	Q_PROPERTY(QString userId READ userId NOTIFY changed)
	Q_PROPERTY(QString errorKey READ errorKey NOTIFY changed)
	Q_PROPERTY(bool loading READ loading NOTIFY changed)
	Q_PROPERTY(bool saving READ saving NOTIFY changed)
	Q_PROPERTY(bool fromCache READ fromCache NOTIFY changed)
	Q_PROPERTY(bool offline READ offline NOTIFY changed)
	Q_PROPERTY(bool pendingSync READ pendingSync NOTIFY changed)
	Q_PROPERTY(bool syncing READ syncing NOTIFY changed)
	Q_PROPERTY(bool syncBlocked READ syncBlocked NOTIFY changed)
	Q_PROPERTY(QString syncErrorKey READ syncErrorKey NOTIFY changed)

public:
	explicit Avm_Profile(AUsers_Repository *in_repository, AsNetwork_Monitor *in_network, AProfile_Sync *in_sync,
	                     QObject *in_parent = 0);
	QVariantMap profile() const;
	QString userId() const;
	QString errorKey() const;
	bool loading() const;
	bool saving() const;
	bool fromCache() const;
	bool offline() const;
	bool pendingSync() const;
	bool syncing() const;
	bool syncBlocked() const;
	QString syncErrorKey() const;
	Q_INVOKABLE void reload(const QString &in_user_id);
	Q_INVOKABLE bool saveProfile(const QVariantMap &in_changes);
	Q_INVOKABLE void retrySync();

signals:
	void changed();
	void profileSaved();

private slots:
	void on_User_Loaded(int in_request_id, const SUsers_User &in_user, bool in_from_cache, const AApi_Error &in_error);
	void on_Profile_Changed(const QString &in_user_id);
	void on_Network_Changed();
	void on_Connection_Restored();

private:
	bool Build_Changes(const QVariantMap &in_values, QJsonObject &out_changes);
	void Set_Profile(const SUsers_Profile &in_profile);
	AUsers_Repository *Repository_Instance;
	AsNetwork_Monitor *Network_Instance;
	AProfile_Sync *Sync_Instance;
	QVariantMap Profile_Data;
	QString User_ID;
	QString Error_Key;
	int Load_Request_ID = 0;
	bool Saving_Flag = false;
	bool From_Cache = false;
};
//-------------------------------------------------------------------------------------------------------------
