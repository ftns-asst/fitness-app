#pragma once

#include <QEvent>
#include <QNetworkInformation>
#include <QObject>
#include <QString>

//-------------------------------------------------------------------------------------------------------------
// Состояние сети ОС. Unknown не запрещает запросы; доступность API определяется HTTP-ответами.
//-------------------------------------------------------------------------------------------------------------
class AsNetwork_Monitor : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QString status READ status NOTIFY changed)
	Q_PROPERTY(bool offline READ offline NOTIFY changed)
	Q_PROPERTY(bool available READ available NOTIFY changed)

public:
	explicit AsNetwork_Monitor(QObject *in_parent = 0, bool in_auto_start = true);
	bool eventFilter(QObject *in_watched, QEvent *in_event) override;
	QString status() const;
	bool offline() const;
	bool available() const;
	void Update_State(QNetworkInformation::Reachability in_reachability, bool in_captive, bool in_available);

signals:
	void changed();
	void connectionRestored();

private slots:
	void Refresh_State();

private:
	QString Current_Status = "unknown";
	bool Is_Offline = false;
	bool Is_Available = false;
};
//-------------------------------------------------------------------------------------------------------------
