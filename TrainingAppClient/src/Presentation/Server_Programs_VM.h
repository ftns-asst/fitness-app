#pragma once

#include <QObject>
#include <QVariant>

class AsAuth_Session;
class QJsonObject;
struct SHttp_Response;

//-------------------------------------------------------------------------------------------------------------
// Черновики, созданные из серверного каталога, принадлежат текущей сессии.
//-------------------------------------------------------------------------------------------------------------
class Avm_Server_Programs : public QObject
{
	Q_OBJECT
	Q_PROPERTY(bool authenticated READ authenticated NOTIFY changed)
	Q_PROPERTY(bool busy READ busy NOTIFY changed)
	Q_PROPERTY(QString errorKey READ errorKey NOTIFY changed)
	Q_PROPERTY(QVariantList drafts READ drafts NOTIFY changed)
	Q_PROPERTY(QVariantMap draft READ draft NOTIFY changed)

public:
	explicit Avm_Server_Programs(AsAuth_Session *in_session, QObject *in_parent = 0);

	bool authenticated() const;
	bool busy() const;
	QString errorKey() const;
	QVariantList drafts() const;
	QVariantMap draft() const;

	Q_INVOKABLE void loadDrafts();
	Q_INVOKABLE void loadDraft(const QString &in_id);
	Q_INVOKABLE void createDraft(const QString &in_template_id, const QVariantList &in_days);
	Q_INVOKABLE void saveRecords(const QString &in_id, const QVariantList &in_records);

signals:
	void changed();
	void accountChanged();
	void draftCreated(const QString &programId);

private:
	enum class EOperation
	{
		List,
		Detail,
		Create,
		Records
	};

	void Send_Read(EOperation in_operation, const QString &in_path);
	void Send_Post(EOperation in_operation, const QString &in_path, const QJsonObject &in_body);
	void Handle_Response(int in_id, const SHttp_Response &in_response);
	void Reset_Session();

	AsAuth_Session *Session;
	QVariantList Drafts;
	QVariantMap Draft;
	QString Error_Key;
	QString Draft_Id;
	EOperation Operation = EOperation::List;
	int Request_Id = 0;
};
//-------------------------------------------------------------------------------------------------------------
