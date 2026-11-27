#include "Presentation/List_Model.h"

//-------------------------------------------------------------------------------------------------------------
AList_Model::AList_Model(QObject *in_parent) : QAbstractListModel(in_parent)
{
}
//-------------------------------------------------------------------------------------------------------------
void AList_Model::Set_Roles(const QStringList &in_role_names)
{
	beginResetModel();

	Role_Name_List = in_role_names;
	Role_Name_Hash.clear();

	for (int i = 0; i < Role_Name_List.size(); ++i)
		Role_Name_Hash.insert(Qt::UserRole + 1 + i, Role_Name_List.at(i).toUtf8());

	endResetModel();
}
//-------------------------------------------------------------------------------------------------------------
void AList_Model::Set_Rows(const QVector<QVariantMap> &in_rows)
{
	if (Rows == in_rows)
		return;

	const bool count_changed = Rows.size() != in_rows.size();
	beginResetModel();

	// Копия: вызывающий может переиспользовать свой контейнер, а модель
	// обязана оставаться согласованной с тем, что она отдаёт.
	Rows = in_rows;

	endResetModel();
	// Сигнал после сброса модели: подписчики count обязаны увидеть новое число
	// строк уже согласованным с данными.
	if (count_changed)
		emit countChanged();
}
//-------------------------------------------------------------------------------------------------------------
void AList_Model::Append_Rows(const QVector<QVariantMap> &in_rows)
{
	if (in_rows.isEmpty())
		return;

	const int first = Rows.size();
	beginInsertRows(QModelIndex(), first, first + in_rows.size() - 1);
	Rows.append(in_rows);
	endInsertRows();
	emit countChanged();
}
//-------------------------------------------------------------------------------------------------------------
void AList_Model::Update_Rows(const QVector<QVariantMap> &in_rows, const QString &in_id_role)
{
	const int common_count = qMin(Rows.size(), in_rows.size());

	for (int i = 0; i < common_count; ++i)
	{
		if (!Rows.at(i).contains(in_id_role) || !in_rows.at(i).contains(in_id_role) ||
		    Rows.at(i).value(in_id_role) != in_rows.at(i).value(in_id_role))
		{
			Set_Rows(in_rows);
			return;
		}
	}

	for (int i = 0; i < common_count; ++i)
	{
		if (Rows.at(i) == in_rows.at(i))
			continue;

		Rows[i] = in_rows.at(i);
		emit dataChanged(index(i), index(i));
	}

	if (in_rows.size() > Rows.size())
		Append_Rows(in_rows.mid(Rows.size()));
	else if (in_rows.size() < Rows.size())
	{
		beginRemoveRows(QModelIndex(), in_rows.size(), Rows.size() - 1);
		Rows.resize(in_rows.size());
		endRemoveRows();
		emit countChanged();
	}
}
//-------------------------------------------------------------------------------------------------------------
int AList_Model::Row_Count() const
{
	return Rows.size();
}
//-------------------------------------------------------------------------------------------------------------
int AList_Model::Role_Count() const
{
	return Role_Name_List.size();
}
//-------------------------------------------------------------------------------------------------------------
QString AList_Model::Role_Name(int in_role) const
{
	for (int i = 0; i < Role_Name_List.size(); ++i)
	{
		if (Qt::UserRole + 1 + i == in_role)
			return Role_Name_List.at(i);
	}

	return QString();
}
//-------------------------------------------------------------------------------------------------------------
QVariant AList_Model::Data(int in_row, int in_role) const
{
	QString name = Role_Name(in_role);

	if (name.isEmpty() || in_row < 0 || in_row >= Rows.size())
		return QVariant();

	return Rows.at(in_row).value(name);
}
//-------------------------------------------------------------------------------------------------------------
// Вне диапазона возвращается пустая карта, а не пустые значения по именам: QML
// отличил бы «поле есть и пустое» от «строки нет» только по ключам.
QVariantMap AList_Model::get(int in_row) const
{
	if (in_row < 0 || in_row >= Rows.size())
		return QVariantMap();

	return Rows.at(in_row);
}
//-------------------------------------------------------------------------------------------------------------
int AList_Model::rowCount(const QModelIndex &in_parent) const
{
	return in_parent.isValid() ? 0 : Rows.size();
}
//-------------------------------------------------------------------------------------------------------------
QVariant AList_Model::data(const QModelIndex &in_index, int in_role) const
{
	return Data(in_index.row(), in_role);
}
//-------------------------------------------------------------------------------------------------------------
QHash<int, QByteArray> AList_Model::roleNames() const
{
	return Role_Name_Hash;
}
//-------------------------------------------------------------------------------------------------------------
