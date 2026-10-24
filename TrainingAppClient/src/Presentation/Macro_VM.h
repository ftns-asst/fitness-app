#pragma once

#include "Domain/Macro_Calculator.h"

#include <QDate>
#include <QObject>
#include <QString>
#include <QStringList>

class AMeasurement_Repository;
class AUsers_Repository;

//----------------------------------------------------------------------------
// Экран авторасчёта КБЖУ «Рассчитать» (спринт S7, issue 50).
//
// Считает через AsMacro_Calculator и хранит параметры пользователя в
// profile_local (миграция v5). Вес берётся из последнего замера тела —
// спрашивать его отдельно было бы вторым источником правды о весе, и со временем
// они разошлись бы.
//
// Формулы и коэффициенты живут в домене, а не здесь: экран не должен уметь их
// считать иначе, чем это делает тест.
//----------------------------------------------------------------------------
class Avm_Macro : public QObject
{
	Q_OBJECT

	Q_PROPERTY(bool hasResult READ Has_Result NOTIFY changed)
	// Числа результата. Отдельные свойства, а не один словарь: подписка на
	// ключ объекта в QML ненадёжна, а пересчёт подписывается на changed целиком.
	Q_PROPERTY(double bmrKcal READ bmr_kcal NOTIFY changed)
	Q_PROPERTY(double tdeeKcal READ tdee_kcal NOTIFY changed)
	Q_PROPERTY(double caloriesKcal READ calories_kcal NOTIFY changed)
	Q_PROPERTY(double proteinG READ protein_g NOTIFY changed)
	Q_PROPERTY(double fatG READ fat_g NOTIFY changed)
	Q_PROPERTY(double carbsG READ carbs_g NOTIFY changed)
	// Введённые параметры — чтобы форма показала их после перезахода.
	Q_PROPERTY(double weightKg READ weight_kg NOTIFY changed)
	Q_PROPERTY(double heightCm READ height_cm NOTIFY changed)
	Q_PROPERTY(bool isFemale READ is_female NOTIFY changed)
	Q_PROPERTY(QString activityLevel READ activity_level NOTIFY changed)
	Q_PROPERTY(QString goal READ goal NOTIFY changed)
	Q_PROPERTY(bool hasWeight READ Has_Weight NOTIFY changed)
	Q_PROPERTY(bool hasHeight READ Has_Height NOTIFY changed)
	Q_PROPERTY(bool hasBirthDate READ Has_Birth_Date NOTIFY changed)
	// Дата рождения строкой YYYY-MM-DD: DateField на экране оперирует строкой,
	// и без неё экран тащил бы QDate в разметку.
	Q_PROPERTY(QString birthDate READ birth_date NOTIFY changed)
	Q_PROPERTY(QString errorKey READ error_key NOTIFY changed)
	// Дата последнего расчёта (ISO). Нужна главной и экрану расчёта, чтобы
	// отличить свежие числа от прошлых: сами числа живут в profile_local.
	Q_PROPERTY(QString calculatedAt READ calculated_at NOTIFY changed)

public:
	Avm_Macro(AsMacro_Calculator *in_calculator, AMeasurement_Repository *in_measurement_repository,
	          AUsers_Repository *in_users_repository, QObject *in_parent = 0);

	// Обёртки QML-моста: имена в нижнем регистре первой буквы (§2 CODE_STYLE).
	Q_INVOKABLE bool load(const QString &in_user_id);
	// Сохраняет введённые параметры и пересчитывает. Один вызов, а не
	// «сохранить» + «посчитать»: человек ждёт результата сразу после ввода, и
	// два вызова из разметки разошлись бы по порядку.
	Q_INVOKABLE bool calculate(const QString &in_user_id, double in_weight_kg, double in_height_cm, bool in_is_female,
	                           const QString &in_birth_date, const QString &in_activity_level, const QString &in_goal);

	bool Load(const QString &in_user_id);
	bool Calculate(const QString &in_user_id, double in_weight_kg, double in_height_cm, bool in_is_female,
	               const QString &in_birth_date, const QString &in_activity_level, const QString &in_goal);

	bool Has_Result() const;
	double bmr_kcal() const;
	double tdee_kcal() const;
	double calories_kcal() const;
	double protein_g() const;
	double fat_g() const;
	double carbs_g() const;
	double weight_kg() const;
	double height_cm() const;
	bool is_female() const;
	QString activity_level() const;
	QString goal() const;
	bool Has_Weight() const;
	bool Has_Height() const;
	bool Has_Birth_Date() const;
	QString birth_date() const;
	QString error_key() const;
	QString calculated_at() const;

	// Режимы активности для выбора на экране: ключ и подпись отдаются парой,
	// иначе экран держал бы свою таблицу ключей рядом с формулами.
	Q_INVOKABLE QStringList activityLevels() const;
	Q_INVOKABLE QString activityCaption(const QString &in_level) const;

	// Фиксированная дата для тестов: возраст считается от неё.
	Q_INVOKABLE void setToday(const QString &in_date);
	void Set_Today(const QString &in_date);

signals:
	void changed();

private:
	void Clear_Result();
	void Clear_Error();
	// Вес для расчёта: введённый, иначе последний замер тела. Ноль означает
	// «неизвестно» — и это не то же самое, что вес 0 кг.
	double Resolve_Weight(double in_weight_kg) const;

	AsMacro_Calculator *Calculator_Instance = 0;
	AMeasurement_Repository *Measurement_Repository_Instance = 0;
	AUsers_Repository *Users_Repository_Instance = 0;

	AsMacro_Calculator::SResult Result;
	QString Birth_Date_Text;
	QString Activity_Level_Text;
	QString Goal_Text;
	// Дата последнего расчёта: пустая, если расчёта ещё не было.
	QString Calculated_At_Text;
	QString Error_Key_Text;
	double Weight_Value = 0;
	double Height_Value = 0;
	bool Female_Value = false;
	bool Has_Weight_Value = false;
	bool Has_Height_Value = false;
	// Дата, от которой считается возраст. Явная, а не currentDate: тест не должен
	// зависеть от часов машины.
	QString Today_Text;
};
//----------------------------------------------------------------------------