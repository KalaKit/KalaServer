/*
auto headerEnd = readBuffer.find("\r\n\r\n");

//incomplete headers
if (headerEnd == string::npos) break;

size_t headerSize = headerEnd + 4;

//extract header block
string headerblock = readBuffer.substr(0, headerSize);

//parse content length

size_t contentLength{};
auto clPos = headerblock.find("Content-Length:");
if (clPos != string::npos)
{
	size_t valueStart = clPos + 15;
	size_t valueEnd = headerblock.find("\r\n", valueStart);
	string value = headerblock.substr(valueStart, valueEnd - valueStart);

	contentLength = scast<size_t>(stoul(value));
}

size_t totalRequired = headerSize + contentLength;

//wait until full body is present (if any)
if (readBuffer.size() < totalRequired) break;

string fullRequest = readBuffer.substr(0, totalRequired);

string newLine = 
	!fullRequest.empty() && fullRequest.back() != '\n'
	? "\n"
	: "";

if (fullRequest.size() > MAX_TOTAL_PAYLOAD_SIZE_BYTES)
{
	sendMsg = "Max payload size '" + to_string(MAX_TOTAL_PAYLOAD_SIZE_BYTES) + "' was reached, cannot accept bigger payload!";

	Log::Print(
		connectionIP + sendMsg,
		"CONNECTION_SOCKET",
		LogType::LOG_WARNING);
	
	Response::SendResponse({
		.responseType = ResponseType::R_413,
		.contentType = ContentType::CT_HTML,
		.optionalSendTypes = { OptionalSendType::S_FORCE_CLOSE },
		.responseBody = 
			ReturnErrorBody(sendMsg,
			ResponseType::R_413),
		.connection = raw
	});

	break;
}

//remove processed request from buffer
readBuffer.erase(0, totalRequired);

if (readBuffer.size() > 0)
{
	Log::Print(
		"There is '" + to_string(readBuffer.size()) + "' bytes of data remaining after removing the total required bytes from the readbuffer.",
		"CONNECTION_LOOP",
		LogType::LOG_INFO);
}

string fullRequestToLog = fullRequest;
if (fullRequestToLog.ends_with("\r\n\r\n")) fullRequestToLog.erase(fullRequestToLog.size() - 4);
if (fullRequestToLog.ends_with("\r\n")) fullRequestToLog.erase(fullRequestToLog.size() - 2);

Log::Print(
		"------------------------------\n"
		+ connectionIP + "Parsing client request (" + to_string(bytesReceived) + " bytes):\n"
		+ fullRequestToLog
		+ "\n------------------------------");

//
// PARSE HEADER AND BODY CONTENT
//

RequestData req{};

bool foundGetLineError{};
{
	size_t headerEnd = fullRequest.find("\r\n\r\n");
	string headerBlock = fullRequest.substr(0, headerEnd);

	req.body = (headerEnd != string::npos)
		? fullRequest.substr(headerEnd + 4)
		: "";

	istringstream stream(headerBlock);
	string line{};

	if (getline(stream, line))
	{
		if (!line.empty()
			&& line.back() == '\r')
		{
			line.pop_back();
		}

		istringstream firstLine(line);
		firstLine >> req.method >> req.domainRoute.route >> req.httpVersion;

		req.method = ToUpperString(req.method);
		req.domainRoute.route = ToLowerString(req.domainRoute.route);
		req.httpVersion = ToUpperString(req.httpVersion);

		if (req.method.empty())
		{
			sendMsg = "Payload did not contain any method!";

			Log::Print(
				connectionIP + sendMsg,
				"CONNECTION_SOCKET",
				LogType::LOG_WARNING);

			Response::SendResponse({
				.responseType = ResponseType::R_400,
				.contentType = ContentType::CT_HTML,
				.responseBody = 
					ReturnErrorBody(sendMsg,
					ResponseType::R_400),
				.connection = raw
			});

			break;
		}
		if (req.method != "GET")
		{
			sendMsg = "Method '" + req.method + "' is not supported!";

			Log::Print(
				connectionIP + sendMsg,
				"CONNECTION_SOCKET",
				LogType::LOG_WARNING);

			Response::SendResponse({
				.responseType = ResponseType::R_405,
				.contentType = ContentType::CT_HTML,
				.responseBody = 
					ReturnErrorBody(sendMsg,
					ResponseType::R_405),
				.connection = raw
			});

			break;
		}

		if (req.domainRoute.route.empty())
		{
			sendMsg = "Payload did not contain a route!";

			Log::Print(
				connectionIP + sendMsg,
				"CONNECTION_SOCKET",
				LogType::LOG_WARNING);

			Response::SendResponse({
				.responseType = ResponseType::R_400,
				.contentType = ContentType::CT_HTML,
				.responseBody = 
					ReturnErrorBody(sendMsg,
					ResponseType::R_400),
				.connection = raw
			});

			break;
		}

		if (req.httpVersion.empty())
		{
			sendMsg = "Payload did not contain any http version!";

			Log::Print(
				connectionIP + sendMsg,
				"CONNECTION_SOCKET",
				LogType::LOG_WARNING);

			Response::SendResponse({
				.responseType = ResponseType::R_400,
				.contentType = ContentType::CT_HTML,
				.responseBody = 
					ReturnErrorBody(sendMsg,
					ResponseType::R_400),
				.connection = raw
			});

			break;
		}
		if (req.httpVersion != "HTTP/1.1")
		{
			sendMsg = "HTTP version '" + req.httpVersion + "' is not supported!";

			Log::Print(
				connectionIP + sendMsg,
				"CONNECTION_SOCKET",
				LogType::LOG_WARNING);

			Response::SendResponse({
				.responseType = ResponseType::R_400,
				.contentType = ContentType::CT_HTML,
				.responseBody = 
					ReturnErrorBody(sendMsg,
					ResponseType::R_400),
				.connection = raw
			});

			break;
		}
	}

	while (getline(stream, line))
	{
		if (!line.empty()
			&& line.back() == '\r')
		{
			line.pop_back();
		}

		if (line.empty()) continue;

		size_t colon = line.find(':');
		if (colon == string::npos)
		{
			sendMsg = "Payload headers are malformed!";

			Log::Print(
				connectionIP + sendMsg,
				"CONNECTION_SOCKET",
				LogType::LOG_WARNING);

			Response::SendResponse({
				.responseType = ResponseType::R_400,
				.contentType = ContentType::CT_HTML,
				.responseBody = 
					ReturnErrorBody(sendMsg,
					ResponseType::R_400),
				.connection = raw
			});

			foundGetLineError = true;
			break;
		}

		string key = line.substr(0, colon);
		string value = line.substr(colon + 1);

		key = ToLowerString(TrimString(key));
		value = TrimString(value);

		if (key == "host")
		{
			if (!req.domainRoute.domain.empty())
			{
				sendMsg = "Payload contained more than one 'host' field!";

				Log::Print(
					connectionIP + sendMsg,
					"CONNECTION_SOCKET",
					LogType::LOG_WARNING);

				Response::SendResponse({
					.responseType = ResponseType::R_400,
					.contentType = ContentType::CT_HTML,
					.responseBody = 
					ReturnErrorBody(sendMsg,
					ResponseType::R_400),
					.connection = raw
				});

				foundGetLineError = true;
				break;
			}

			req.domainRoute.domain = ToLowerString(value);
		}
		else
		{
			auto it = req.headers.find(key);
			if (it != req.headers.end())
			{
				if (ContainsValue(allowedDuplicateHeaders, key))
				{
					it->second += ", " + value;
				}
				else
				{
					sendMsg = "Payload contained more than one '" + key + "' field!";

					Log::Print(
						connectionIP + sendMsg,
						"CONNECTION_LOOP",
						LogType::LOG_ERROR,
						2);

					Response::SendResponse({
						.responseType = ResponseType::R_400,
						.contentType = ContentType::CT_HTML,
						.responseBody = 
							ReturnErrorBody(sendMsg,
							ResponseType::R_400),
						.connection = raw
					});

					foundGetLineError = true;
					break;
				}
			}
			else req.headers.emplace(std::move(key), std::move(value));
		}
	}
}

if (foundGetLineError) break;

//
// VERIFY HOST
//

if (req.domainRoute.domain.empty())
{
	sendMsg = "Payload did not contain host!";

	Log::Print(
		connectionIP + sendMsg,
		"CONNECTION_SOCKET",
		LogType::LOG_WARNING);

	Response::SendResponse({
		.responseType = ResponseType::R_400,
		.contentType = ContentType::CT_HTML,
		.responseBody = 
			ReturnErrorBody(sendMsg,
			ResponseType::R_400),
		.connection = raw
	});

	break;
}

if (req.domainRoute.domain.starts_with("http://")) req.domainRoute.domain.erase(0, 7);
if (req.domainRoute.domain.starts_with("https://")) req.domainRoute.domain.erase(0, 8);
if (req.domainRoute.domain.starts_with("www.")) req.domainRoute.domain.erase(0, 4);

bool foundDomain{};
if (req.domainRoute.domain == serverIPDomain 
	|| req.domainRoute.domain == serverIPPortDomain)
{
	foundDomain = true;
}
else
{
	size_t dcolon = req.domainRoute.domain.find(':');
	if (dcolon != string::npos) req.domainRoute.domain.erase(dcolon);

	for (const auto& d : KalaServerCore::GetServerDomains())
	{
		if (req.domainRoute.domain == d)
		{
			foundDomain = true;
			break;
		}
	}
}

if (!foundDomain)
{
	sendMsg = "Host '" + req.domainRoute.domain + "' was not found!";

	Log::Print(
		connectionIP + sendMsg,
		"CONNECTION_SOCKET",
		LogType::LOG_WARNING);

	Response::SendResponse({
		.responseType = ResponseType::R_400,
		.contentType = ContentType::CT_HTML,
		.responseBody = 
			ReturnErrorBody(sendMsg,
			ResponseType::R_400),
		.connection = raw
	});

	break;
}
	
//
// PARSE ROUTE
//

if (req.domainRoute.route.starts_with("http://")) req.domainRoute.route.erase(0, 7);
if (req.domainRoute.route.starts_with("https://")) req.domainRoute.route.erase(0, 8);
if (req.domainRoute.route.starts_with("www.")) req.domainRoute.route.erase(0, 4);
if (req.domainRoute.route.starts_with(serverIPPortDomain)) req.domainRoute.route.erase(0, serverIPPortDomain.size());
if (req.domainRoute.route.starts_with(serverIPDomain)) req.domainRoute.route.erase(0, serverIPDomain.size());

if (!req.domainRoute.route.starts_with('/')) req.domainRoute.route.insert(req.domainRoute.route.begin(), '/');

mutex& m_routes = KalaServerCore::GetRoutesMutex();
mutex& m_blacklistedKeywords = KalaServerCore::GetBlacklistedKeywordsMutex();

lockwait_m(m_routes);
lockwait_m(m_blacklistedKeywords);

const vector<DomainRoute>& routes = KalaServerCore::GetRoutes();

string blacklistedKeyword{};
for (const auto& b : KalaServerCore::GetBlacklistedKeywords())
{
	if (req.domainRoute.route.find(b) != string::npos)
	{
		blacklistedKeyword = b;
		break;
	}
}
if (!blacklistedKeyword.empty())
{
	KalaServerCore::BanIP(raw->connectionIP);

	Log::Print(
		"[ " + raw->connectionIP + " ] User was banned for trying to access route via blacklisted keyword '" + blacklistedKeyword + "'",
		"CONNECTION_LOOP",
		LogType::LOG_INFO);

	unlock_m(m_blacklistedKeywords);
	unlock_m(m_routes);

	Response::SendResponse({
		.responseType = ResponseType::R_418,
		.contentType = ContentType::CT_HTML,
		.optionalSendTypes = { OptionalSendType::S_FORCE_CLOSE },
		.responseBody = 
			ReturnErrorBody("Get banned nerd",
			ResponseType::R_418),
		.connection = raw
	});

	break;
}

bool foundValidRoute{};
for (const auto& r : routes)
{
	if (foundValidRoute) break;

	if (r.route == req.domainRoute.route)
	{
		foundValidRoute = true;
		break;
	}
}

if (!foundValidRoute)
{
	unlock_m(m_blacklistedKeywords);
	unlock_m(m_routes);

	sendMsg = "Route '" + req.domainRoute.route + "' was not found!";

	Log::Print(
		connectionIP + sendMsg,
		"CONNECTION_SOCKET",
		LogType::LOG_WARNING);

	Response::SendResponse({
		.responseType = ResponseType::R_404,
		.contentType = ContentType::CT_HTML,
		.responseBody = 
			ReturnErrorBody(sendMsg,
			ResponseType::R_404),
		.connection = raw
	});

	break;
}

unlock_m(m_blacklistedKeywords);
unlock_m(m_routes);

//
// ALLOW CONNECTION
//

vector<OptionalSendType> optSendTypes{};
for (const auto& [k, v] : req.headers)
{
	if (k == "connection")
	{
		string lowerValue = ToLowerString(v);
		if (lowerValue.find("close") != string::npos)
		{
			optSendTypes.push_back(OptionalSendType::S_FORCE_CLOSE);
			break;
		}
	}
}

raw->requestData = std::move(req);

Log::Print(
	"[ " + raw->connectionIP + " ] Connection verified, sending response.",
	"CONNECTION_LOOP",
	LogType::LOG_INFO);

Response::SendResponse({
	.responseType = ResponseType::R_200,
	.contentType = ContentType::CT_HTML,
	.optionalSendTypes = optSendTypes,
	.responseBody = string(response_success),
	.connection = raw
});
*/