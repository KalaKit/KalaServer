//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <WS2tcpip.h>
#include <memory>
#include <sstream>
#include <string>

#include "ssl.h" //openssl
#include "err.h" //openssl

#include "core/event.hpp"

using KalaKit::Core::Event;
using KalaKit::Core::EventType;
using KalaKit::Core::EmailData;
using KalaKit::Core::PrintData;

using std::unique_ptr;
using std::make_unique;
using std::ostringstream;
using std::string;

static bool SendEmail(const EmailData& emailData);

namespace KalaKit::Core
{
	void Event::SendEvent(EventType type, const EmailData& emailData)
	{
		if (type != EventType::event_send_email)
		{

		}
		SendEmail(emailData);
	}
}

static bool SendEmail(const EmailData& emailData)
{
	auto fail = [&](const string& reason)
	{
		PrintData emailErrorData =
		{
			.indentationLength = 2,
			.addTimeStamp = true,
			.customTag = "SEND_EMAIL",
			.message = reason
		};
		unique_ptr<Event> emailErrorEvent = make_unique<Event>();
		emailErrorEvent->SendEvent(EventType::event_print_message, emailErrorData);
	};

	auto base64_encode = [](const string& input) -> string
	{
		BIO* bio = BIO_new(BIO_s_mem());
		BIO* b64 = BIO_new(BIO_f_base64());
		BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
		b64 = BIO_push(b64, bio);
		BIO_write(b64, input.data(), static_cast<int>(input.size()));
		BIO_flush(b64);
		BUF_MEM* buffer{};
		BIO_get_mem_ptr(b64, &buffer);
		string output(buffer->data, buffer->length);
		BIO_free_all(b64);
		return output;
	};

	auto send_ssl = [](SSL* ssl, const string& msg) -> bool
	{
		string formatted = msg + "\r\n";
		return SSL_write(ssl, formatted.c_str(), static_cast<int>(formatted.size())) > 0;
	};

	auto recv_ssl = [](SSL* ssl) -> string
	{
		char buf[2048] = {};
		int len = SSL_read(ssl, buf, sizeof(buf) - 1);
		return len > 0 ? string(buf, len) : "";
	};

	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		fail("WSAStartup failed!");
		return false;
	}

	addrinfo hints = {}, * res = nullptr;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	if (getaddrinfo(emailData.smtpServer.c_str(), "587", &hints, &res) != 0)
	{
		fail("Failed to resolve SMTP server!");
		WSACleanup();
		return false;
	}

	SOCKET sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (connect(sock, res->ai_addr, static_cast<int>(res->ai_addrlen)) == SOCKET_ERROR)
	{
		fail("Connection to SMTP server failed!");
		closesocket(sock);
		WSACleanup();
		return false;
	}

	freeaddrinfo(res);

	char recvBuf[2048] = {};
	recv(sock, recvBuf, sizeof(recvBuf) - 1, 0);

	string ehlo = "EHLO localhost\r\n";
	send(sock, ehlo.c_str(), static_cast<int>(ehlo.size()), 0);
	recv(sock, recvBuf, sizeof(recvBuf) - 1, 0);

	string starttls = "STARTTLS\r\n";
	send(sock, starttls.c_str(), static_cast<int>(starttls.size()), 0);
	recv(sock, recvBuf, sizeof(recvBuf) - 1, 0);

	SSL_library_init();
	SSL_load_error_strings();
	auto ctx = unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)>(SSL_CTX_new(TLS_client_method()), SSL_CTX_free);
	if (!ctx)
	{
		fail("Failed to create SSL context!");
		return false;
	}

	SSL* ssl = SSL_new(ctx.get());
	SSL_set_fd(ssl, static_cast<int>(sock));
	if (SSL_connect(ssl) != 1)
	{
		fail("SSL handshake failed!");
		SSL_free(ssl);
		closesocket(sock);
		WSACleanup();
		return false;
	}

	bool success = true;
	auto check = [&](const string& cmd)
	{
		if (!send_ssl(ssl, cmd) || recv_ssl(ssl).starts_with("5"))
		{
			fail("SMTP command failed: " + cmd);
			success = false;
		}
		return success;
	};

	if (!check("EHLO localhost"))
	{
		fail("SMTP EHLO command failed!");
		return false;
	}
	if (!check("AUTH LOGIN"))
	{
		fail("SMTP AUTH LOGIN failed!");
		return false;
	}
	if (!check(base64_encode(emailData.username)))
	{
		fail("SMTP username authentication failed!");
		return false;
	}
	if (!check(base64_encode(emailData.password)))
	{
		fail("SMTP password authentication failed!");
		return false;
	}
	if (!check("MAIL FROM:<" + emailData.sender + ">"))
	{
		fail("SMTP MAIL FROM command failed!");
		return false;
	}
	for (const auto& r : emailData.receivers)
	{
		if (!check("RCPT TO:<" + r + ">"))
		{
			fail("SMTP RCPT TO failed for: " + r);
			return false;
		}
	}
	if (!check("DATA"))
	{
		fail("SMTP DATA command failed!");
		return false;
	}

	ostringstream msg{};
	msg << "Subject: " << emailData.subject << "\r\n";
	msg << "From: " << emailData.sender << "\r\n";
	msg << "To: ";
	for (size_t i = 0; i < emailData.receivers.size(); ++i)
	{
		msg << emailData.receivers[i];
		if (i + 1 < emailData.receivers.size()) msg << ", ";
	}
	msg << "\r\n\r\n" << emailData.body << "\r\n.\r\n";

	if (!check(msg.str())) return false;
	send_ssl(ssl, "QUIT");

	SSL_shutdown(ssl);
	SSL_free(ssl);
	closesocket(sock);
	WSACleanup();

	PrintData emailSuccessData =
	{
		.indentationLength = 0,
		.addTimeStamp = true,
		.customTag = "SEND_EMAIL",
		.message = "Email '" + emailData.subject + "' was successfully sent!"
	};
	unique_ptr<Event> emailSuccessEvent = make_unique<Event>();
	emailSuccessEvent->SendEvent(EventType::event_print_message, emailSuccessData);

	return success;
}