#include "Presentation/Server_Programs_VM.h"

#include "Data/Auth/Auth_Session.h"

#include <QJsonArray>
#include <QSet>
#include <QUuid>

namespace
{
QString Wire_ID(const QString &in_id)
{
	const QUuid id(in_id);
	return id.isNull() ? QString() : id.toString(QUuid::WithoutBraces);
}
} // namespace

//-------------------------------------------------------------------------------------------------------------
Avm_Server_Programs::Avm_Server_Programs(AsAuth_Session *in_session, QObject *in_parent)
    : QObject(in_parent), Session(in_session)
{
	connect(Session, &AsAuth_Session::requestFinished, this, &Avm_Server_Programs::Handle_Response);
	connect(Session, &AsAuth_Session::sessionChanged, this, &Avm_Server_Programs::Reset_Session);
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Server_Programs::authenticated() const
{
	return Session->Has_Session();
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Server_Programs::busy() const
{
	return Request_Id > 0;
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Server_Programs::errorKey() const
{
	return Error_Key;
}
//-------------------------------------------------------------------------------------------------------------
QVariantList Avm_Server_Programs::drafts() const
{
	return Drafts;
}
//-------------------------------------------------------------------------------------------------------------
QVariantMap Avm_Server_Programs::draft() const
{
	return Draft;
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Server_Programs::loadDrafts()
{
	if (busy())
		return;
	Drafts.clear();
	Send_Read(EOperation::List, "/programs");
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Server_Programs::loadDraft(const QString &in_id)
{
	if (busy())
		return;
	Draft_Id = Wire_ID(in_id);
	Draft.clear();
	if (Draft_Id.isEmpty())
	{
		Error_Key = "validation_error";
		emit changed();
		return;
	}
	Send_Read(EOperation::Detail, "/programs/" + Draft_Id);
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Server_Programs::createDraft(const QString &in_template_id, const QVariantList &in_days)
{
	if (busy())
		return;
	const QString template_id = Wire_ID(in_template_id);
	QSet<int> selected;
	QJsonArray days;
	for (const QVariant &value : in_days)
	{
		const int day = value.toInt();
		if (value.toDouble() == day && day >= 1 && day <= 7 && !selected.contains(day))
		{
			selected.insert(day);
			days.append(day);
		}
	}
	if (template_id.isEmpty() || days.isEmpty() || days.size() != in_days.size())
	{
		Error_Key = "validation_error";
		emit changed();
		return;
	}
	Send_Post(EOperation::Create, "/programs/from-default",
	          {{"default_program_id", template_id}, {"days_of_week", days}});
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Server_Programs::saveRecords(const QString &in_id, const QVariantList &in_records)
{
	if (busy())
		return;
	const QString user_id = Wire_ID(Session->User_ID());
	QSet<QString> required;
	for (const QVariant &value : Draft.value("required_measurements").toList())
		required.insert(Wire_ID(value.toMap().value("exercise_id").toString()));

	QJsonArray records;
	for (const QVariant &value : in_records)
	{
		const QVariantMap row = value.toMap();
		const QString exercise_id = Wire_ID(row.value("exercise_id").toString());
		const double weight = row.value("weight_kg").toDouble();
		bool reps_ok;
		const QVariant reps_value = row.value("reps", 1);
		const int reps = reps_value.toInt(&reps_ok);
		if (exercise_id.isEmpty() || !required.remove(exercise_id) || !(weight > 0 && weight < 10000) || !reps_ok ||
		    reps < 1 || reps_value.toDouble() != reps)
			break;
		records.append(QJsonObject{{"exercise_id", exercise_id}, {"weight_kg", weight}, {"reps", reps}});
	}
	if (user_id.isEmpty() || Draft_Id.isEmpty() || Wire_ID(in_id) != Draft_Id || Draft.isEmpty() || records.isEmpty() ||
	    records.size() > 100 || records.size() != in_records.size())
	{
		Error_Key = authenticated() ? QString("validation_error") : QString("auth_required");
		emit changed();
		return;
	}
	Send_Post(EOperation::Records, "/users/" + user_id + "/prs/bulk", {{"records", records}});
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Server_Programs::Send_Read(EOperation in_operation, const QString &in_path)
{
	SHttp_Request request;
	request.Path = in_path;
	request.Retry_Enabled = true;
	Operation = in_operation;
	Request_Id = Session->Send_Authed(request);
	Error_Key = Request_Id > 0 ? QString() : QString("auth_required");
	emit changed();
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Server_Programs::Send_Post(EOperation in_operation, const QString &in_path, const QJsonObject &in_body)
{
	SHttp_Request request;
	request.Method = "POST";
	request.Path = in_path;
	request.Body = QJsonDocument(in_body).toJson(QJsonDocument::Compact);
	request.Headers.insert("Content-Type", "application/json");
	Operation = in_operation;
	Request_Id = Session->Send_Authed(request);
	Error_Key = Request_Id > 0 ? QString() : QString("auth_required");
	emit changed();
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Server_Programs::Handle_Response(int in_id, const SHttp_Response &in_response)
{
	if (in_id != Request_Id)
		return;
	Request_Id = 0;
	if (!in_response.Success)
	{
		const bool uncertain = in_response.HTTP_Status_Code == 0 || in_response.HTTP_Status_Code >= 500;
		Error_Key = Operation == EOperation::Create && uncertain ? QString("program_create_unconfirmed")
		                                                         : in_response.Error.Key();
	}
	else if (Operation == EOperation::Records)
	{
		// Даже если тело успешного ответа потерялось, состояние берётся у программы.
		loadDraft(Draft_Id);
		return;
	}
	else if (Operation == EOperation::List)
	{
		if (!in_response.JSON.isArray())
			Error_Key = "undefined_error";
		else
		{
			for (const QJsonValue &value : in_response.JSON.array())
			{
				const QJsonObject row = value.toObject();
				const QString state = row.value("state").toString();
				if (row.value("source_kind").toString() == "default" &&
				    (state == "draft" || state == "needs_measurement") &&
				    !Wire_ID(row.value("id").toString()).isEmpty())
					Drafts.append(row.toVariantMap());
			}
		}
	}
	else
	{
		const QJsonObject row = in_response.JSON.object();
		const QString id = Wire_ID(row.value("id").toString());
		if (id.isEmpty() || (Operation == EOperation::Detail && id != Draft_Id) ||
		    !row.value("required_measurements").isArray())
			Error_Key =
			    Operation == EOperation::Create ? QString("program_create_unconfirmed") : QString("undefined_error");
		else
		{
			const bool created = Operation == EOperation::Create;
			Draft_Id = id;
			Draft = row.toVariantMap();
			emit changed();
			if (created)
				emit draftCreated(id);
			return;
		}
	}
	emit changed();
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Server_Programs::Reset_Session()
{
	Request_Id = 0;
	Drafts.clear();
	Draft.clear();
	Draft_Id.clear();
	Error_Key.clear();
	emit changed();
	emit accountChanged();
}
//-------------------------------------------------------------------------------------------------------------
