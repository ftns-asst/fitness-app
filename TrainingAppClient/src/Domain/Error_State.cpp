#include "Domain/Error_State.h"

#include <QLoggingCategory>

//----------------------------------------------------------------------------
AError_State::AError_State(const QLoggingCategory *in_log_category) : Log_Category(in_log_category)
{
}
//----------------------------------------------------------------------------
QString AError_State::Get_Last_Error() const
{
	return Error_Text;
}
//----------------------------------------------------------------------------
bool AError_State::Has_Error() const
{
	return Error_Text.isEmpty() == false;
}
//----------------------------------------------------------------------------
bool AError_State::Set_Last_Error(const QString &in_error)
{
	Error_Text = in_error;

	if (Log_Category != 0)
		qCWarning(*Log_Category).noquote() << in_error;

	return false;
}
//----------------------------------------------------------------------------
void AError_State::Clear_Last_Error()
{
	Error_Text.clear();
}
//----------------------------------------------------------------------------