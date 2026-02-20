//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

#include <unordered_map>
#include <atomic>

#include "KalaHeaders/core_utils.hpp"
#include "KalaHeaders/log_utils.hpp"

#include "server/ks_response.hpp"
#include "server/ks_connect.hpp"
#include "server/ks_server.hpp"

using KalaHeaders::KalaCore::ToVar;
using KalaHeaders::KalaCore::FromVar;
using KalaHeaders::KalaCore::RemoveDuplicates;

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaServer::Server::UNASSIGNED_SOCKET_VALUE;
using KalaServer::Server::ResponseData;
using KalaServer::Server::ResponseType;
using KalaServer::Server::ContentType;
using KalaServer::Server::OptionalSendType;
using KalaServer::Server::Response;
using KalaServer::Server::ServerCore;

using std::unordered_map;
using std::string;
using std::string_view;
using std::to_string;
using std::vector;
using std::memory_order_acquire;
using std::memory_order_release;

static void Send(const ResponseData& data, bool socketAlreadyClosed);

namespace KalaServer::Server
{
    static const unordered_map<OptionalSendType, string_view> optionalSendTypes
    {
        {OptionalSendType::S_DOWNLOAD,
            "Content-Disposition: attachment; filename="},
        {OptionalSendType::S_NO_CACHE,
            "Cache-Control: no-store, no-cache, must-revalidate\r\n"
            "Pragma: no-cache\r\n"
            "Expires: 0\r\n"},
        {OptionalSendType::S_FORCE_CLOSE, 
            "Connection: close\r\n"}
    };

    static const unordered_map<ResponseType, string_view> responseTypes
    {
      { ResponseType::R_200, "HTTP/1.1 200 OK" },
      { ResponseType::R_204, "HTTP/1.1 204 No Content" },
      { ResponseType::R_206, "HTTP/1.1 206 Partial Content" },

      { ResponseType::R_400, "HTTP/1.1 400 Bad Request" },
      { ResponseType::R_401, "HTTP/1.1 401 Unauthorized" },
      { ResponseType::R_404, "HTTP/1.1 404 Not Found" },
      { ResponseType::R_405, "HTTP/1.1 405 Method Not Allowed" },
      { ResponseType::R_413, "HTTP/1.1 413 Payload Too Large" },
      { ResponseType::R_418, "HTTP/1.1 418 I'm a teapot" },

      { ResponseType::R_500, "HTTP/1.1 500 Internal Server Error" },
      { ResponseType::R_503, "HTTP/1.1 503 Service Unavailable" }
    };

    struct ContentTypeData
    {
        string_view extension{};
        string_view mimeType{};
    };

    static const unordered_map<ContentType, ContentTypeData> contentTypes
    {
        { ContentType::CT_HTML, { ".html", "text/html" } },
        { ContentType::CT_CSS,  { ".css",  "text/css" } },
        { ContentType::CT_JS,   { ".js",   "application/javascript" } },

        { ContentType::CT_PNG,     { ".png",  "image/png" } },
        { ContentType::CT_JPEG,    { ".jpeg", "image/jpeg" } },
        { ContentType::CT_JPG,     { ".jpg",  "image/jpeg" } },
        { ContentType::CT_GIF,     { ".gif",  "image/gif" } },
        { ContentType::CT_WEBP,    { ".webp", "image/webp" } },
        { ContentType::CT_ICO,     { ".ico",  "image/vnd.microsoft.icon" } },
        { ContentType::CT_SVG_XML, { ".svg",  "image/svg+xml" } },

        { ContentType::CT_MP3,  { ".mp3",  "audio/mpeg" } },
        { ContentType::CT_M4A,  { ".m4a",  "audio/mp4" } },
        { ContentType::CT_AAC,  { ".aac",  "audio/aac" } },
        { ContentType::CT_OGG,  { ".ogg",  "audio/ogg" } },
        { ContentType::CT_OPUS, { ".opus", "audio/opus" } },
        { ContentType::CT_FLAC, { ".flac", "audio/flac" } },
        { ContentType::CT_WAV,  { ".wav",  "audio/wav" } },

        { ContentType::CT_MP4,  { ".mp4",  "video/mp4" } },
        { ContentType::CT_WEBM, { ".webm", "video/webm" } },
        { ContentType::CT_MKV,  { ".mkv",  "video/x-matroska" } },
        { ContentType::CT_MOV,  { ".mov",  "video/quicktime" } },
        { ContentType::CT_AVI,  { ".avi",  "video/x-msvideo" } },

        { ContentType::CT_WOFF,  { ".woff",  "font/woff" } },
        { ContentType::CT_WOFF2, { ".woff2", "font/woff2" } },
        { ContentType::CT_TTF,   { ".ttf",   "font/ttf" } },
        { ContentType::CT_OTF,   { ".otf",   "font/otf" } },

        { ContentType::CT_PLAIN,    { ".txt",  "text/plain" } },
        { ContentType::CT_JSON,     { ".json", "application/json" } },
        { ContentType::CT_XML,      { ".xml",  "application/xml" } },
        { ContentType::CT_CSV,      { ".csv",  "text/csv" } },
        { ContentType::CT_MARKDOWN, { ".md",   "text/markdown" } },

        { ContentType::CT_ZIP,  { ".zip",  "application/zip" } },
        { ContentType::CT_GZ,   { ".gz",   "application/gzip" } },
        { ContentType::CT_TAR,  { ".tar",  "application/x-tar" } },
        { ContentType::CT_7Z,   { ".7z",   "application/x-7z-compressed" } },
        { ContentType::CT_RAR,  { ".rar",  "application/vnd.rar" } },
        { ContentType::CT_PDF,  { ".pdf",  "application/pdf" } },
        { ContentType::CT_WASM, { ".wasm", "application/wasm" } },

        { ContentType::CT_OCTET, { "", "application/octet-stream" } }
    };

    void Response::SendResponse(const ResponseData& data)
    {
        auto close_socket = [&data]()
            {
                if (data.connection->connectionSocket.load(memory_order_acquire) == UNASSIGNED_SOCKET_VALUE)
                {
                    Log::Print(
                        "Cannot close socket because its unassigned!",
                        "SEND_RESPONSE",
                        LogType::LOG_ERROR,
                        2);

                    return;
                }

#ifdef _WIN32
                SOCKET csock = data.connection 
                    ? ToVar<SOCKET>(data.connection->connectionSocket.load(memory_order_acquire))
                    : ToVar<SOCKET>(data.connectionSocket.load(memory_order_acquire));

                shutdown(csock, SD_BOTH);
                closesocket(csock);
#else
                int csock = data.connection 
                    ? ToVar<int>(data.connection->connectionSocket.load(memory_order_acquire))
                    : ToVar<int>(data.connectionSocket.load(memory_order_acquire));

                shutdown(csock, SHUT_RDWR);
                close(csock);
#endif
            };

        bool noTargetSocket = 
            !data.connection 
            && data.connectionSocket.load(memory_order_acquire) == UNASSIGNED_SOCKET_VALUE;
        bool invalidResponseType = data.responseType == ResponseType::R_INVALID;
        bool invalidContentType = data.contentType == ContentType::CT_INVALID;
        bool emptyBody = data.responseBody.empty();

        if (noTargetSocket)
        {
            Log::Print(
                "Failed to send response because no Connection struct or connectionSocket has been assigned!",
                "SEND_RESPONSE",
                LogType::LOG_ERROR,
                2);

            return;
        }
        if (invalidResponseType)
        {
            Log::Print(
                "Failed to send response because response type was invalid or unassigned!",
                "SEND_RESPONSE",
                LogType::LOG_ERROR,
                2);

            close_socket();

            return;
        }
        if (invalidContentType)
        {
            Log::Print(
                "Failed to send response because content type was invalid or unassigned!",
                "SEND_RESPONSE",
                LogType::LOG_ERROR,
                2);

            close_socket();

            return;
        }
        if (emptyBody)
        {
            Log::Print(
                "No response body was assigned, using placeholder.",
                "SEND_RESPONSE",
                LogType::LOG_WARNING);

            Send(data, true);

            close_socket();

            return;
        }

        Send(data, false);
    }

    OptionalSendType Response::StringToSendType(string_view input)
    {
        for (const auto& [k, v] : optionalSendTypes)
        {
            if (v == input) return k;
        }

        return OptionalSendType::S_INVALID;
    }
    string_view Response::SendTypeToString(OptionalSendType type)
    {
        auto it = optionalSendTypes.find(type);
        if (it == optionalSendTypes.end()) return {};

        return it->second;
    }

    ContentType Response::ExtensionToContentType(string_view input)
    {
        for (const auto& [k, v] : contentTypes)
        {
            if (v.extension == input) return k;
        }

        return ContentType::CT_INVALID;
    }
    string_view Response::ContentTypeToExtension(ContentType type)
    {
        auto it = contentTypes.find(type);
        if (it == contentTypes.end()) return {};

        return it->second.extension;
    }

    ContentType Response::MimeTypeToContentType(string_view input)
    {
        for (const auto& [k, v] : contentTypes)
        {
            if (v.mimeType == input) return k;
        }

        return ContentType::CT_INVALID;
    }
    string_view Response::ContentTypeToMimeType(ContentType type)
    {
        auto it = contentTypes.find(type);
        if (it == contentTypes.end()) return {};

        return it->second.mimeType;
    }

    ResponseType Response::StringToResponseType(string_view input)
    {
        for (const auto& [k, v] : responseTypes)
        {
            if (v == input) return k;
        }

        return ResponseType::R_INVALID;
    }
    string_view Response::ResponseTypeToString(ResponseType type)
    {
        auto it = responseTypes.find(type);
        if (it == responseTypes.end()) return {};

        return it->second;
    }
}

void Send(const ResponseData& data, bool socketAlreadyClosed)
{
    const string_view statusLine = Response::ResponseTypeToString(data.responseType);
    const string_view contentType = Response::ContentTypeToMimeType(data.contentType);
    static constexpr string_view ending = "\r\n";

    vector<OptionalSendType> localSendTypes = data.optionalSendTypes;
    RemoveDuplicates(localSendTypes);

    bool containsCloseSendType{};

    vector<string_view> sendTypes{};
    for (const auto& st : localSendTypes)
    {
        if (st == OptionalSendType::S_FORCE_CLOSE) containsCloseSendType = true;
        string_view result = Response::SendTypeToString(st);

        sendTypes.push_back(result);
    }

    string responseLogContent = 
        "Status line: " + string(statusLine) + ",\n"
        + "Content type: " + string(contentType) + ",\n"
        + "Content length: " + to_string(data.responseBody.size());

    string fullResponse = 
        string(statusLine) + string(ending)
        + "Content-Type: " + string(contentType) + string(ending) 
        + "Content-Length: " + to_string(data.responseBody.size()) + string(ending);

    for (string_view st: sendTypes)
    {
        if (!st.empty())
        {
            fullResponse += st;

            responseLogContent += ", " + string(st);
        }
    }
    
    fullResponse += string(ending);

    fullResponse += data.responseBody;

    auto send_all = [&data, &fullResponse, &responseLogContent]() -> bool
        {
            int totalSent{};
            int length = fullResponse.size();
#ifdef _WIN32
            SOCKET csock = data.connection 
                ? ToVar<SOCKET>(data.connection->connectionSocket.load(memory_order_acquire))
                : ToVar<SOCKET>(data.connectionSocket.load(memory_order_acquire));

            while (totalSent < length)
            {
                int sent = send(
                    csock,
                    fullResponse.data() + totalSent,
                    length - totalSent,
                    0);

                if (sent == SOCKET_ERROR)
                {
                    string err = to_string(WSAGetLastError());

                    Log::Print(
                        "Failed to finish sending server '" + ServerCore::GetServerName() + "' response '" + responseLogContent + "'! Reason: " + err,
                        "SEND_RESPONSE",
                        LogType::LOG_ERROR,
                        2);

                    return false;
                }

                if (sent == 0)
                {
                    Log::Print(
                        "Failed to finish sending server '" + ServerCore::GetServerName() + "' response '" + responseLogContent + "' because the connection closed!",
                        "SEND_RESPONSE",
                        LogType::LOG_ERROR,
                        2);

                    return false;
                }

                totalSent += sent;
            }
#else
            int csock = csock = data.connection 
                ? ToVar<int>(data.connection->connectionSocket.load(memory_order_acquire))
                : ToVar<int>(data.connectionSocket.load(memory_order_acquire));

            while (totalSent < length)
            {
                ssize_t sent = send(
                    csock,
                    fullResponse.data() + totalSent,
                    length - totalSent,
                    MSG_NOSIGNAL);

                if (sent < 0)
                {
                    //interrupted by signal, retry
                    if (errno == EINTR) continue;

                    string err = string(strerror(errno)) + " (errno=" + to_string(errno) + ")";

                    Log::Print(
                        "Failed to finish sending server '" + ServerCore::GetServerName() + "' response '" + responseLogContent + "'! Reason: " + err,
                        "SEND_RESPONSE",
                        LogType::LOG_ERROR,
                        2);

                    return false;
                }

                if (sent == 0)
                {
                    Log::Print(
                        "Failed to finish sending server '" + ServerCore::GetServerName() + "' response '" + responseLogContent + "' because the connection closed!",
                        "SEND_RESPONSE",
                        LogType::LOG_ERROR,
                        2);

                    return false;
                }

                totalSent += scast<int>(sent);
            }
#endif

            return true;
        };

    if (send_all())
    {
        Log::Print(
            "[ " + data.connection->connectionIP + " ] Sent response:\n" + responseLogContent + ".",
            "SEND_RESPONSE",
            LogType::LOG_SUCCESS);
    }

    if (!socketAlreadyClosed
        && containsCloseSendType)
    {
#ifdef _WIN32
        SOCKET csock = ToVar<SOCKET>(data.connection->connectionSocket.load(memory_order_acquire));
        closesocket(csock);
#else
        int csock = ToVar<int>(data.connection->connectionSocket.load(memory_order_acquire));
        close(csock);
#endif
    }
}