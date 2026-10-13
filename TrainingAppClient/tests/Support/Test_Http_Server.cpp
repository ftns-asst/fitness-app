#include "Support/Test_Http_Server.h"

#include <QHostAddress>
#include <QTcpSocket>
#include <QTimer>
#include <QVariant>

//----------------------------------------------------------------------------
// ATest_Http_Server
//----------------------------------------------------------------------------
ATest_Http_Server::ATest_Http_Server(QObject *in_parent) : QObject(in_parent), Server(this)
{
	connect(&Server, &QTcpServer::newConnection, this, &ATest_Http_Server::onNewConnection);
}
//----------------------------------------------------------------------------
ATest_Http_Server::~ATest_Http_Server()
{
	Stop();
}
//----------------------------------------------------------------------------
bool ATest_Http_Server::Start()
{
	Last_Error_Text.clear();

	if (Server.isListening())
		return true;

	if (Server.listen(QHostAddress::LocalHost, 0) == false)
	{
		Last_Error_Text = Server.errorString();
		return false;
	}

	return true;
}
//----------------------------------------------------------------------------
void ATest_Http_Server::Stop()
{
	QList<QTcpSocket *> sockets;
	int i;

	sockets = Buffers.keys();

	for (i = 0; i < sockets.size(); ++i)
	{
		sockets.at(i)->abort();
	}

	Buffers.clear();
	Server.close();
}
//----------------------------------------------------------------------------
bool ATest_Http_Server::Is_Listening() const
{
	return Server.isListening();
}
//----------------------------------------------------------------------------
QUrl ATest_Http_Server::Base_URL() const
{
	return QUrl(QString("http://127.0.0.1:%1").arg(Server.serverPort()));
}
//----------------------------------------------------------------------------
QString ATest_Http_Server::Last_Error() const
{
	return Last_Error_Text;
}
//----------------------------------------------------------------------------
void ATest_Http_Server::Enqueue_Response(const STest_Http_Response &in_response)
{
	Responses.append(in_response);
}
//----------------------------------------------------------------------------
int ATest_Http_Server::Pending_Response_Count() const
{
	return Responses.size();
}
//----------------------------------------------------------------------------
int ATest_Http_Server::Request_Count() const
{
	return Requests.size();
}
//----------------------------------------------------------------------------
STest_Http_Request ATest_Http_Server::Request_At(int in_index) const
{
	STest_Http_Request empty_request;

	if (in_index < 0 || in_index >= Requests.size())
		return empty_request;

	return Requests.at(in_index);
}
//----------------------------------------------------------------------------
void ATest_Http_Server::Clear()
{
	Responses.clear();
	Requests.clear();
	Last_Error_Text.clear();
}
//----------------------------------------------------------------------------
void ATest_Http_Server::onNewConnection()
{
	QTcpSocket *socket;

	while (Server.hasPendingConnections())
	{
		socket = Server.nextPendingConnection();
		Buffers.insert(socket, QByteArray());
		connect(socket, &QTcpSocket::readyRead, this, &ATest_Http_Server::onSocketReadyRead);
		connect(socket, &QTcpSocket::disconnected, this, &ATest_Http_Server::onSocketDisconnected);
	}
}
//----------------------------------------------------------------------------
void ATest_Http_Server::onSocketReadyRead()
{
	QTcpSocket *socket;

	socket = qobject_cast<QTcpSocket *>(sender());

	if (socket == 0 || Buffers.contains(socket) == false)
		return;

	Buffers[socket].append(socket->readAll());

	if (Buffers.value(socket).size() > 1024 * 1024)
	{
		Send_Bad_Request(socket);
		return;
	}

	Try_Consume_Request(socket);
}
//----------------------------------------------------------------------------
void ATest_Http_Server::onSocketDisconnected()
{
	QTcpSocket *socket;

	socket = qobject_cast<QTcpSocket *>(sender());

	if (socket == 0)
		return;

	Buffers.remove(socket);
	socket->deleteLater();
}
//----------------------------------------------------------------------------
void ATest_Http_Server::onDelayedResponse()
{
	QTimer *timer;
	QTcpSocket *socket;
	QByteArray response;

	timer = qobject_cast<QTimer *>(sender());

	if (timer == 0)
		return;

	socket = qobject_cast<QTcpSocket *>(timer->parent());
	response = timer->property("httpResponse").toByteArray();
	timer->deleteLater();

	if (socket == 0 || socket->state() == QAbstractSocket::UnconnectedState)
		return;

	Write_And_Close(socket, response);
}
//----------------------------------------------------------------------------
bool ATest_Http_Server::Try_Consume_Request(QTcpSocket *in_socket)
{
	QByteArray buffer;
	QByteArray header_block;
	QByteArray line;
	QByteArray name;
	QByteArray value;
	QList<QByteArray> lines;
	QList<QByteArray> request_parts;
	STest_Http_Request request;
	STest_Http_Response response;
	qint64 content_length;
	int colon_pos;
	int header_end;
	int i;
	bool length_ok;

	buffer = Buffers.value(in_socket);
	header_end = buffer.indexOf("\r\n\r\n");

	if (header_end < 0)
		return false;

	header_block = buffer.left(header_end);
	lines = header_block.split('\n');

	if (lines.isEmpty())
	{
		Send_Bad_Request(in_socket);
		return false;
	}

	line = lines.takeFirst().trimmed();
	request_parts = line.split(' ');

	if (request_parts.size() != 3 || request_parts.at(2).startsWith("HTTP/") == false)
	{
		Send_Bad_Request(in_socket);
		return false;
	}

	request.Method = request_parts.at(0);
	request.Target = request_parts.at(1);
	content_length = 0;

	for (i = 0; i < lines.size(); ++i)
	{
		line = lines.at(i).trimmed();

		if (line.isEmpty())
			continue;

		colon_pos = line.indexOf(':');

		if (colon_pos <= 0)
		{
			Send_Bad_Request(in_socket);
			return false;
		}

		name = line.left(colon_pos).trimmed().toLower();
		value = line.mid(colon_pos + 1).trimmed();
		request.Headers.insert(name, value);
	}

	if (request.Headers.contains("content-length"))
	{
		content_length = request.Headers.value("content-length").toLongLong(&length_ok);

		if (length_ok == false || content_length < 0 || content_length > 1024 * 1024)
		{
			Send_Bad_Request(in_socket);
			return false;
		}
	}

	if (buffer.size() < header_end + 4 + content_length)
		return false;

	request.Body = buffer.mid(header_end + 4, content_length);
	Requests.append(request);
	Buffers.remove(in_socket);
	emit requestReceived();

	if (Responses.isEmpty())
	{
		response.Status_Code = 500;
		response.Body = "{\"key\":\"undefined_error\",\"message\":\"No fake response queued\"}";
	}
	else
	{
		response = Responses.takeFirst();
	}

	Send_Response(in_socket, response);

	return true;
}
//----------------------------------------------------------------------------
void ATest_Http_Server::Send_Bad_Request(QTcpSocket *in_socket)
{
	STest_Http_Response response;

	Buffers.remove(in_socket);
	response.Status_Code = 400;
	response.Body = "{\"key\":\"validation_error\",\"message\":\"Malformed HTTP request\"}";
	Send_Response(in_socket, response);
}
//----------------------------------------------------------------------------
void ATest_Http_Server::Send_Response(QTcpSocket *in_socket, const STest_Http_Response &in_response)
{
	QTimer *timer;
	QByteArray response;

	if (in_response.Disconnect_Without_Response)
	{
		in_socket->abort();
		return;
	}

	response = Build_Response(in_response);

	if (in_response.Delay_Ms <= 0)
	{
		Write_And_Close(in_socket, response);
		return;
	}

	timer = new QTimer(in_socket);
	timer->setSingleShot(true);
	timer->setProperty("httpResponse", response);
	connect(timer, &QTimer::timeout, this, &ATest_Http_Server::onDelayedResponse);
	timer->start(in_response.Delay_Ms);
}
//----------------------------------------------------------------------------
void ATest_Http_Server::Write_And_Close(QTcpSocket *in_socket, const QByteArray &in_response)
{
	in_socket->write(in_response);
	in_socket->disconnectFromHost();
}
//----------------------------------------------------------------------------
QByteArray ATest_Http_Server::Build_Response(const STest_Http_Response &in_response)
{
	QByteArray response;
	QMap<QByteArray, QByteArray>::const_iterator iterator;

	response = "HTTP/1.1 " + QByteArray::number(in_response.Status_Code) + " " +
	           Reason_Phrase(in_response.Status_Code) + "\r\n";
	response += "Content-Length: " + QByteArray::number(in_response.Body.size()) + "\r\n";
	response += "Connection: close\r\n";

	if (in_response.Content_Type.isEmpty() == false)
		response += "Content-Type: " + in_response.Content_Type + "\r\n";

	iterator = in_response.Headers.constBegin();

	while (iterator != in_response.Headers.constEnd())
	{
		response += iterator.key() + ": " + iterator.value() + "\r\n";
		++iterator;
	}

	response += "\r\n";
	response += in_response.Body;

	return response;
}
//----------------------------------------------------------------------------
QByteArray ATest_Http_Server::Reason_Phrase(int in_status_code)
{
	switch (in_status_code)
	{
	case 200:
		return "OK";
	case 201:
		return "Created";
	case 204:
		return "No Content";
	case 400:
		return "Bad Request";
	case 401:
		return "Unauthorized";
	case 404:
		return "Not Found";
	case 429:
		return "Too Many Requests";
	case 500:
		return "Internal Server Error";
	case 502:
		return "Bad Gateway";
	case 503:
		return "Service Unavailable";
	default:
		return "Test Response";
	}
}
//----------------------------------------------------------------------------