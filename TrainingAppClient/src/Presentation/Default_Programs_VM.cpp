#include "Presentation/Default_Programs_VM.h"

#include "Data/Network/Http_Client.h"

#include <QJsonArray>
#include <QUrlQuery>
#include <QUuid>

namespace
{
bool Has_Program_Identity(const QJsonObject &in_object)
{
	return !QUuid(in_object.value("id").toString()).isNull() && !in_object.value("name").toString().trimmed().isEmpty();
}
bool Is_Program_Detail(const QJsonObject &in_object)
{
	const QJsonValue workouts_value = in_object.value("workouts");
	const int week_count = in_object.value("weeks_count").toInt();
	if (!Has_Program_Identity(in_object) || !workouts_value.isArray() || week_count < 1 || week_count > 52)
		return false;

	const QJsonArray workouts = workouts_value.toArray();
	for (const QJsonValue &value : workouts)
	{
		const QJsonObject workout = value.toObject();
		const int week = workout.value("week_number").toInt();
		const QJsonObject workout_template = workout.value("template").toObject();
		if (week < 1 || week > week_count || !workout_template.value("exercises").isArray())
			return false;
	}
	return true;
}
} // namespace

//-------------------------------------------------------------------------------------------------------------
Avm_Default_Programs::Avm_Default_Programs(AsHttp_Client *in_http, QObject *in_parent)
    : QObject(in_parent), HTTP_Client(in_http)
{
	connect(HTTP_Client, &AsHttp_Client::requestFinished, this, &Avm_Default_Programs::Handle_Response);
}
//-------------------------------------------------------------------------------------------------------------
QVariantList Avm_Default_Programs::programs() const
{
	return Programs;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Default_Programs::loading() const
{
	return Catalog_Request > 0;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Default_Programs::loaded() const
{
	return Catalog_Loaded;
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Default_Programs::errorKey() const
{
	return Catalog_Error;
}
//-------------------------------------------------------------------------------------------------------------
QVariantMap Avm_Default_Programs::detail() const
{
	return Detail;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Default_Programs::detailLoading() const
{
	return Detail_Request > 0;
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Default_Programs::detailErrorKey() const
{
	return Detail_Error;
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Default_Programs::load(const QString &in_goal, const QString &in_level, const QVariantList &in_days)
{
	QUrlQuery query;
	if (!in_goal.isEmpty())
		query.addQueryItem("goal", in_goal);
	if (!in_level.isEmpty())
		query.addQueryItem("level", in_level);
	for (const QVariant &day : in_days)
		query.addQueryItem("day_of_week", QString::number(day.toInt()));

	QUrl url("/default-programs");
	if (!query.isEmpty())
		url.setQuery(query);
	Catalog_Request = HTTP_Client->Get(url.toString(QUrl::FullyEncoded));
	Programs.clear();
	Catalog_Loaded = false;
	Catalog_Error = Catalog_Request > 0 ? QString() : QString("undefined_error");
	emit catalogChanged();
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Default_Programs::loadDetail(const QString &in_id)
{
	const QUuid id(in_id);
	Detail_Request = id.isNull() ? 0 : HTTP_Client->Get("/default-programs/" + id.toString(QUuid::WithoutBraces));
	Detail.clear();
	Detail_Error = Detail_Request > 0 ? QString() : QString("undefined_error");
	emit detailChanged();
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Default_Programs::Handle_Response(int in_id, const SHttp_Response &in_response)
{
	if (in_id == Catalog_Request)
	{
		Catalog_Request = 0;
		if (!in_response.Success)
			Catalog_Error = in_response.Error.Key();
		else if (!in_response.JSON.isArray())
			Catalog_Error = "undefined_error";
		else
		{
			const QJsonArray rows = in_response.JSON.array();
			for (const QJsonValue &row : rows)
			{
				if (!Has_Program_Identity(row.toObject()))
				{
					Catalog_Error = "undefined_error";
					break;
				}
			}
			if (Catalog_Error.isEmpty())
			{
				Programs = rows.toVariantList();
				Catalog_Loaded = true;
			}
		}
		emit catalogChanged();
	}
	else if (in_id == Detail_Request)
	{
		Detail_Request = 0;
		const QJsonObject object = in_response.JSON.object();
		if (!in_response.Success)
			Detail_Error = in_response.Error.Key();
		else if (!Is_Program_Detail(object))
			Detail_Error = "undefined_error";
		else
			Detail = object.toVariantMap();
		emit detailChanged();
	}
}
//-------------------------------------------------------------------------------------------------------------
