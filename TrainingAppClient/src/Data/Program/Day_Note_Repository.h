#pragma once

#include "Data/Program/Program_Repository_Base.h"
#include "Domain/Program_Model.h"

#include <QVector>

class ADay_Note_Repository : public AProgram_Repository_Base
{
	Q_OBJECT

public:
	explicit ADay_Note_Repository(AsSql_Database *in_database, QObject *in_parent = 0);

	bool Load_Range(const QString &in_user_id, const QString &in_date_from, const QString &in_date_to,
	                QVector<Program_Model::SDay_Note> &out_notes);
	bool Save_Note(const Program_Model::SDay_Note &in_note);

private:
	bool Is_Valid_Date(const QString &in_date) const;
};
