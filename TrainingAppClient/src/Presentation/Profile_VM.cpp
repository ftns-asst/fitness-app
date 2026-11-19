#include "Presentation/Profile_VM.h"

#include <QtMath>

//-------------------------------------------------------------------------------------------------------------
Avm_Profile::Avm_Profile(AUsers_Repository *in_repository, AsNetwork_Monitor *in_network, AProfile_Sync *in_sync,
                         QObject *in_parent)
    : QObject(in_parent), Repository_Instance(in_repository), Network_Instance(in_network), Sync_Instance(in_sync)
{
	connect(Repository_Instance, &AUsers_Repository::getUserFinished, this, &Avm_Profile::on_User_Loaded);
	connect(Repository_Instance, &AUsers_Repository::profileChanged, this, &Avm_Profile::on_Profile_Changed);
	connect(Sync_Instance, &AProfile_Sync::changed, this, &Avm_Profile::on_Profile_Changed);
	if (Network_Instance != 0)
	{
		connect(Network_Instance, &AsNetwork_Monitor::changed, this, &Avm_Profile::on_Network_Changed);
		connect(Network_Instance, &AsNetwork_Monitor::connectionRestored, this, &Avm_Profile::on_Connection_Restored);
	}
}
//-------------------------------------------------------------------------------------------------------------
QVariantMap Avm_Profile::profile() const
{
	return Profile_Data;
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Profile::userId() const
{
	return User_ID;
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Profile::errorKey() const
{
	return Error_Key;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Profile::loading() const
{
	return Load_Request_ID > 0;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Profile::saving() const
{
	return Saving_Flag;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Profile::fromCache() const
{
	return From_Cache;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Profile::offline() const
{
	return Network_Instance != 0 && Network_Instance->offline();
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Profile::pendingSync() const
{
	SProfile_Pending pending;
	return !User_ID.isEmpty() && Repository_Instance->Load_Pending_Profile(User_ID, pending);
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Profile::syncing() const
{
	return Sync_Instance->Is_Syncing(User_ID);
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Profile::syncBlocked() const
{
	SProfile_Pending pending;
	return Repository_Instance->Load_Pending_Profile(User_ID, pending) && pending.Is_Blocked;
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Profile::syncErrorKey() const
{
	SProfile_Pending pending;
	return Repository_Instance->Load_Pending_Profile(User_ID, pending) ? pending.Last_Error : QString();
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Profile::reload(const QString &in_user_id)
{
	SUsers_User cached;
	if (in_user_id == User_ID && (loading() || saving()))
		return;

	User_ID = in_user_id;
	Load_Request_ID = 0;
	Saving_Flag = false;
	Profile_Data.clear();
	Error_Key.clear();
	From_Cache = false;
	if (!User_ID.isEmpty())
	{
		if (Repository_Instance->Get_Cached_User(User_ID, cached) && cached.Has_Profile)
		{
			Set_Profile(cached.Profile);
			From_Cache = true;
		}
		if (!offline() && !pendingSync())
		{
			Load_Request_ID = Repository_Instance->Get_User(User_ID, true);
			if (Load_Request_ID <= 0)
				Error_Key = "profile_load_failed";
		}
	}
	emit changed();
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Profile::saveProfile(const QVariantMap &in_changes)
{
	QJsonObject changes;
	if (saving())
		return false;
	Error_Key.clear();
	if (User_ID.isEmpty())
		Error_Key = "auth_required";
	else if (!Build_Changes(in_changes, changes))
		Error_Key = "invalid_profile";
	else
	{
		Saving_Flag = true;
		bool saved = Repository_Instance->Save_Profile_Locally(User_ID, changes);
		Saving_Flag = false;
		if (!saved)
			Error_Key = "local_save_failed";
		else
		{
			Load_Request_ID = 0;
			on_Profile_Changed(User_ID);
			emit profileSaved();
			return true;
		}
	}
	emit changed();

	return false;
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Profile::retrySync()
{
	Sync_Instance->Retry(User_ID);
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Profile::on_User_Loaded(int in_request_id, const SUsers_User &in_user, bool in_from_cache,
                                 const AApi_Error &in_error)
{
	if (in_request_id != Load_Request_ID || in_user.ID != User_ID)
	{
		if (in_request_id != Load_Request_ID)
			return;
		Load_Request_ID = 0;
		Error_Key = in_error.Is_Error() ? in_error.Key() : "profile_load_failed";
		emit changed();
		return;
	}
	Load_Request_ID = 0;
	From_Cache = in_from_cache;
	if (!in_error.Is_Error() || in_from_cache)
	{
		Profile_Data.clear();
		if (in_user.Has_Profile)
			Set_Profile(in_user.Profile);
	}
	Error_Key = in_error.Is_Error() ? in_error.Key() : QString();
	emit changed();
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Profile::on_Profile_Changed(const QString &in_user_id)
{
	if (in_user_id != User_ID)
		return;
	SUsers_User cached;
	if (Repository_Instance->Get_Cached_User(User_ID, cached) && cached.Has_Profile)
		Set_Profile(cached.Profile);
	From_Cache = pendingSync();
	emit changed();
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Profile::on_Network_Changed()
{
	emit changed();
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Profile::on_Connection_Restored()
{
	if (!User_ID.isEmpty() && !saving() && !pendingSync())
		reload(User_ID);
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Profile::Build_Changes(const QVariantMap &in_values, QJsonObject &out_changes)
{
	bool ok;
	double number;
	for (auto iterator = in_values.cbegin(); iterator != in_values.cend(); ++iterator)
	{
		QString key = iterator.key();
		if (key == "gender")
		{
			QString gender = iterator.value().toString();
			if (gender != "male" && gender != "female")
				return false;
			out_changes.insert("gender", gender);
			continue;
		}
		number = iterator.value().toDouble(&ok);
		if (!ok || !qIsFinite(number) || number < 1)
			return false;
		if (key == "age" && number <= 150 && qFloor(number) == number)
			out_changes.insert("age", (int)number);
		else if (key == "heightCm" && number <= 300 && qFloor(number) == number)
			out_changes.insert("height", (int)number);
		else if (key == "weightKg" && number <= 500)
			out_changes.insert("weight", number);
		else
			return false;
	}

	return !out_changes.isEmpty();
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Profile::Set_Profile(const SUsers_Profile &in_profile)
{
	Profile_Data.clear();
	if (in_profile.Age >= 0)
		Profile_Data.insert("age", in_profile.Age);
	if (!in_profile.Gender.isEmpty())
		Profile_Data.insert("gender", in_profile.Gender);
	if (in_profile.Height_Cm > 0)
		Profile_Data.insert("heightCm", in_profile.Height_Cm);
	if (in_profile.Weight_Kg > 0)
		Profile_Data.insert("weightKg", in_profile.Weight_Kg);
}
//-------------------------------------------------------------------------------------------------------------
