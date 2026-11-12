#pragma once

#include "Domain/Program_Template_Service.h"

#include <QVariantMap>

//-------------------------------------------------------------------------------------------------------------
class Avm_Program_Templates : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QStringList measurementKeys READ measurementKeys CONSTANT)
	Q_PROPERTY(QString programId READ programId NOTIFY changed)
	Q_PROPERTY(QString errorKey READ errorKey NOTIFY changed)
	Q_PROPERTY(QString errorDetail READ errorDetail NOTIFY changed)

public:
	explicit Avm_Program_Templates(AProgram_Template_Service *in_service, QObject *in_parent = 0);

	// preset: caloriesMax, proteinMin, carbsMax, fatMax; необязательные waterMinMl, sleepMinHours, stepsMin.
	// measurements: weightKg и <ключ EMeasurement_Key>Cm; пропущенное поле не заполняется автоматически.
	Q_INVOKABLE bool createDraft(const QString &in_user_id, const QString &in_start_date, const QVariantMap &in_preset,
	                             const QVariantMap &in_measurements);
	bool Create_Draft(const QString &in_user_id, const QString &in_start_date, const QVariantMap &in_preset,
	                  const QVariantMap &in_measurements);
	QString programId() const;
	QStringList measurementKeys() const;
	QString errorKey() const;
	QString errorDetail() const;

signals:
	void changed();
	// Только успешно сохранённый черновик. Внешний маршрут открывает существующий редактор по этому id.
	void templateCreated(const QString &programId);

private:
	bool Parse_Request(const QString &in_user_id, const QString &in_start_date, const QVariantMap &in_preset,
	                   const QVariantMap &in_measurements, SPSMF_Template_Request &out_request);
	bool Fail(const QString &in_key, const QString &in_detail);
	static bool Read_Number(const QVariantMap &in_values, const QString &in_key, double &out_value);

	// Не владеет: сервис живёт дольше ViewModel.
	AProgram_Template_Service *Service_Instance;
	QString Created_Program_Id;
	QString Error_Key_Text;
	AError_State Error_State;
};
//-------------------------------------------------------------------------------------------------------------
