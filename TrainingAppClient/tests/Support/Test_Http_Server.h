#pragma once

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>

//----------------------------------------------------------------------------
// Зафиксированный HTTP-ответ fake server. Используется API-тестами для
// сценариев success/error/timeout/disconnect без доступа к внешней сети.
//----------------------------------------------------------------------------
struct STest_Http_Response
{
	int Status_Code = 200;
	QByteArray Body;
	QByteArray Content_Type = "application/json";
	QMap<QByteArray, QByteArray> Headers;
	int Delay_Ms = 0;
	bool Disconnect_Without_Response = false;
};

//----------------------------------------------------------------------------
// Снимок входящего запроса. Имена headers нормализованы в lowercase.
//----------------------------------------------------------------------------
struct STest_Http_Request
{
	QByteArray Method;
	QByteArray Target;
	QMap<QByteArray, QByteArray> Headers;
	QByteArray Body;
};

//----------------------------------------------------------------------------
// Минимальный HTTP/1.1 fake server на loopback. Поддерживает последовательную
// очередь ответов, Content-Length, произвольные status/headers, задержку и
// принудительный disconnect. TLS/chunked/pipelining намеренно вне test scope.
//----------------------------------------------------------------------------
class ATest_Http_Server : public QObject
{
	Q_OBJECT

public:
	explicit ATest_Http_Server(QObject *in_parent = 0);
	~ATest_Http_Server();

	bool Start();
	void Stop();
	bool Is_Listening() const;
	QUrl Base_URL() const;
	QString Last_Error() const;
	void Enqueue_Response(const STest_Http_Response &in_response);
	int Pending_Response_Count() const;
	int Request_Count() const;
	STest_Http_Request Request_At(int in_index) const;
	void Clear();

signals:
	void requestReceived();

private slots:
	void onNewConnection();
	void onSocketReadyRead();
	void onSocketDisconnected();
	void onDelayedResponse();

private:
	bool Try_Consume_Request(QTcpSocket *in_socket);
	void Send_Bad_Request(QTcpSocket *in_socket);
	void Send_Response(QTcpSocket *in_socket, const STest_Http_Response &in_response);
	void Write_And_Close(QTcpSocket *in_socket, const QByteArray &in_response);
	static QByteArray Build_Response(const STest_Http_Response &in_response);
	static QByteArray Reason_Phrase(int in_status_code);

	QTcpServer Server;
	QMap<QTcpSocket *, QByteArray> Buffers;
	QList<STest_Http_Response> Responses;
	QList<STest_Http_Request> Requests;
	QString Last_Error_Text;
};
//----------------------------------------------------------------------------