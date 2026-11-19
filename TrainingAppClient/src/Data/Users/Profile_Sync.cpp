#include "Data/Users/Profile_Sync.h"

//-------------------------------------------------------------------------------------------------------------
AProfile_Sync::AProfile_Sync(AUsers_Repository *in_repository, AsAuth_Session *in_session,
                             AsNetwork_Monitor *in_network, QObject *in_parent)
    : QObject(in_parent), Repository_Instance(in_repository), Session_Instance(in_session), Network_Instance(in_network)
{
	Retry_Timer.setSingleShot(true);
	connect(&Retry_Timer, &QTimer::timeout, this, &AProfile_Sync::Flush);
	connect(Repository_Instance, &AUsers_Repository::profileQueued, this, &AProfile_Sync::Flush);
	connect(Repository_Instance, &AUsers_Repository::profileUpdated, this, &AProfile_Sync::on_Profile_Updated);
	connect(Session_Instance, &AsAuth_Session::sessionChanged, this, &AProfile_Sync::on_Session_Changed);
	if (Network_Instance != 0)
		connect(Network_Instance, &AsNetwork_Monitor::connectionRestored, this, &AProfile_Sync::on_Connection_Restored);
	Flush();
}
//-------------------------------------------------------------------------------------------------------------
bool AProfile_Sync::Is_Syncing(const QString &in_user_id) const
{
	return Request_ID > 0 && in_user_id == In_Flight_User_ID;
}
//-------------------------------------------------------------------------------------------------------------
void AProfile_Sync::Retry(const QString &in_user_id)
{
	if (in_user_id != Session_Instance->User_ID())
		return;

	Repository_Instance->Reset_Profile_Retry(in_user_id);
	emit changed(in_user_id);
	Flush();
}
//-------------------------------------------------------------------------------------------------------------
void AProfile_Sync::Flush()
{
	SProfile_Pending pending;
	QString user_id = Session_Instance->User_ID();
	Retry_Timer.stop();
	if (Request_ID > 0 || user_id.isEmpty() || (Network_Instance != 0 && Network_Instance->offline()))
		return;
	if (!Repository_Instance->Load_Pending_Profile(user_id, pending) || pending.Is_Blocked)
		return;
	qint64 delay_ms = QDateTime::currentDateTimeUtc().msecsTo(pending.Next_Attempt);
	if (pending.Next_Attempt.isValid() && delay_ms > 0)
	{
		Retry_Timer.start((int)qMin((qint64)300000, delay_ms));
		return;
	}
	In_Flight_User_ID = user_id;
	In_Flight_Queue_ID = pending.ID;
	Request_ID = Repository_Instance->Update_Profile(user_id, pending.Changes);
	if (Request_ID <= 0)
		Retry_Timer.start(5000);
	emit changed(user_id);
}
//-------------------------------------------------------------------------------------------------------------
void AProfile_Sync::on_Profile_Updated(int in_request_id, const SUsers_Profile &in_profile, const AApi_Error &in_error)
{
	if (in_request_id != Request_ID)
		return;

	QString user_id = In_Flight_User_ID;
	qint64 queue_id = In_Flight_Queue_ID;
	SProfile_Pending pending;
	Request_ID = 0;
	In_Flight_User_ID.clear();
	bool persisted = true;
	if (!in_error.Is_Error())
		persisted = Repository_Instance->Confirm_Profile_Update(user_id, queue_id, in_profile);
	else if (Session_Instance->User_ID() == user_id)
	{
		int status = in_error.HTTP_Status_Code();
		AApi_Error error = status == 404 ? AApi_Error("profile_not_found", "Profile not found", status, 0) : in_error;
		bool retryable =
		    status == 0 || status == 408 || status == 429 || status >= 500 || (status >= 200 && status < 300);
		Repository_Instance->Load_Pending_Profile(user_id, pending);
		int delay_ms = status == 429 ? 60000 : 5000;
		for (int i = 0; i < qMin(pending.Attempts, 6); ++i)
			delay_ms = qMin(300000, delay_ms * 2);
		persisted = Repository_Instance->Defer_Profile_Update(user_id, queue_id, error, retryable, delay_ms);
	}
	emit changed(user_id);
	if (!persisted)
		Retry_Timer.start(30000);
	else
		Retry_Timer.start(0);
}
//-------------------------------------------------------------------------------------------------------------
void AProfile_Sync::on_Session_Changed()
{
	Retry_Timer.stop();
	Flush();
}
//-------------------------------------------------------------------------------------------------------------
void AProfile_Sync::on_Connection_Restored()
{
	QString user_id = Session_Instance->User_ID();
	SProfile_Pending pending;
	if (!user_id.isEmpty() && Repository_Instance->Load_Pending_Profile(user_id, pending) && !pending.Is_Blocked)
		Repository_Instance->Reset_Profile_Retry(user_id);
	Flush();
}
//-------------------------------------------------------------------------------------------------------------
