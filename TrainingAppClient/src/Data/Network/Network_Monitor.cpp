#include "Data/Network/Network_Monitor.h"

#include <QCoreApplication>

//-------------------------------------------------------------------------------------------------------------
AsNetwork_Monitor::AsNetwork_Monitor(QObject *in_parent, bool in_auto_start) : QObject(in_parent)
{
	if (!in_auto_start)
		return;

	QNetworkInformation::loadDefaultBackend();
	QNetworkInformation *information = QNetworkInformation::instance();
	if (information != 0)
	{
		connect(information, &QNetworkInformation::reachabilityChanged, this, &AsNetwork_Monitor::Refresh_State);
		connect(information, &QNetworkInformation::isBehindCaptivePortalChanged, this,
		        &AsNetwork_Monitor::Refresh_State);
	}
	if (QCoreApplication::instance() != 0)
		QCoreApplication::instance()->installEventFilter(this);

	Refresh_State();
}
//-------------------------------------------------------------------------------------------------------------
bool AsNetwork_Monitor::eventFilter(QObject *in_watched, QEvent *in_event)
{
	if (in_event->type() == QEvent::ApplicationStateChange)
		Refresh_State();

	return QObject::eventFilter(in_watched, in_event);
}
//-------------------------------------------------------------------------------------------------------------
QString AsNetwork_Monitor::status() const
{
	return Current_Status;
}
//-------------------------------------------------------------------------------------------------------------
bool AsNetwork_Monitor::offline() const
{
	return Is_Offline;
}
//-------------------------------------------------------------------------------------------------------------
bool AsNetwork_Monitor::available() const
{
	return Is_Available;
}
//-------------------------------------------------------------------------------------------------------------
void AsNetwork_Monitor::Update_State(QNetworkInformation::Reachability in_reachability, bool in_captive,
                                     bool in_available)
{
	QString status = "unknown";
	if (in_available)
	{
		if (in_captive)
			status = "captive";
		else if (in_reachability == QNetworkInformation::Reachability::Disconnected)
			status = "offline";
		else if (in_reachability == QNetworkInformation::Reachability::Local ||
		         in_reachability == QNetworkInformation::Reachability::Site)
			status = "limited";
		else if (in_reachability == QNetworkInformation::Reachability::Online)
			status = "online";
	}
	if (status == Current_Status && in_available == Is_Available)
		return;

	bool was_offline = Is_Offline;
	Current_Status = status;
	Is_Available = in_available;
	Is_Offline = status == "offline" || status == "limited" || status == "captive";
	emit changed();
	if (was_offline && status == "online")
		emit connectionRestored();
}
//-------------------------------------------------------------------------------------------------------------
void AsNetwork_Monitor::Refresh_State()
{
	QNetworkInformation *information = QNetworkInformation::instance();
	if (information == 0)
		return;

	Update_State(information->reachability(), information->isBehindCaptivePortal(),
	             information->supports(QNetworkInformation::Feature::Reachability));
}
//-------------------------------------------------------------------------------------------------------------
