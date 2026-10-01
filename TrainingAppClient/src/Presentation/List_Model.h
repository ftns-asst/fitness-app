#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>
#include <QVector>

//----------------------------------------------------------------------------
// Базовый список Presentation (срез W8, issue 24/26).
//
// Роли QML не хранятся в структурах домена: модель получает готовые
// QVariantMap с именованными ролями, а база отвечает только за их выдачу
// (`roleNames`, `data`, инвалидация). Маппинг «структура → роли» делают
// конкретные модели §7 `02-program-model.md`, поэтому база не знает ни про
// программы, ни про правила и не меняется вместе с ними.
//
// Имена ролей — в порядке объявления, значения Qt::UserRole+1 и далее: порядок
// объявления в QML-контракте значим для делегатов, поэтому он фиксирован.
//------------------------------------------------------------------------------
class AList_Model : public QAbstractListModel
{
	Q_OBJECT

public:
	explicit AList_Model(QObject *in_parent = 0);

	// Роли модели. Порядок задаёт значения ролей; вызывается до Set_Rows,
	// смена ролей сама инвалидирует модель.
	void Set_Roles(const QStringList &in_role_names);
	void Set_Rows(const QVector<QVariantMap> &in_rows);

	int Row_Count() const;
	int Role_Count() const;
	QString Role_Name(int in_role) const;
	QVariant Data(int in_row, int in_role) const;

	int rowCount(const QModelIndex &in_parent = QModelIndex()) const override;
	QVariant data(const QModelIndex &in_index, int in_role = Qt::DisplayRole) const override;
	QHash<int, QByteArray> roleNames() const override;

private:
	QStringList Role_Name_List;
	QHash<int, QByteArray> Role_Name_Hash;
	QVector<QVariantMap> Rows;
};
//----------------------------------------------------------------------------