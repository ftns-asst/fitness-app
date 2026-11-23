#pragma once

#include <QObject>
#include <QString>
#include <QVariant>

class AsHttp_Client;
struct SHttp_Response;

//-------------------------------------------------------------------------------------------------------------
// Публичный каталог. Список и карточка загружаются независимо.
//-------------------------------------------------------------------------------------------------------------
class Avm_Default_Programs : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QVariantList programs READ programs NOTIFY catalogChanged)
	Q_PROPERTY(bool loading READ loading NOTIFY catalogChanged)
	Q_PROPERTY(bool loaded READ loaded NOTIFY catalogChanged)
	Q_PROPERTY(QString errorKey READ errorKey NOTIFY catalogChanged)
	Q_PROPERTY(QVariantMap detail READ detail NOTIFY detailChanged)
	Q_PROPERTY(bool detailLoading READ detailLoading NOTIFY detailChanged)
	Q_PROPERTY(QString detailErrorKey READ detailErrorKey NOTIFY detailChanged)

public:
	explicit Avm_Default_Programs(AsHttp_Client *in_http, QObject *in_parent = 0);

	QVariantList programs() const;
	bool loading() const;
	bool loaded() const;
	QString errorKey() const;
	QVariantMap detail() const;
	bool detailLoading() const;
	QString detailErrorKey() const;

	Q_INVOKABLE void load(const QString &in_goal, const QString &in_level, const QVariantList &in_days);
	Q_INVOKABLE void loadDetail(const QString &in_id);

signals:
	void catalogChanged();
	void detailChanged();

private:
	void Handle_Response(int in_id, const SHttp_Response &in_response);

	AsHttp_Client *HTTP_Client;
	QVariantList Programs;
	QVariantMap Detail;
	QString Catalog_Error;
	QString Detail_Error;
	int Catalog_Request = 0;
	int Detail_Request = 0;
	bool Catalog_Loaded = false;
};
//-------------------------------------------------------------------------------------------------------------
