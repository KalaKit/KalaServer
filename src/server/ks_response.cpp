//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <unordered_map>

#include "server/ks_response.hpp"

using std::unordered_map;

namespace KalaServer::Server
{
    static const unordered_map<ResponseType, string_view> statusLines
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

    }

    ContentType Response::ExtensionToContentType(string_view input)
    {
        for (const auto& [k, v] : contentTypes)
        {
            if (v.extension == input) return k;
        }

        return ContentType::CT_INVALID;
    }
    ContentType Response::MimeTypeToContentType(string_view input)
    {
        for (const auto& [k, v] : contentTypes)
        {
            if (v.mimeType == input) return k;
        }

        return ContentType::CT_INVALID;
    }

    string_view Response::ContentTypeToExtension(ContentType type)
    {
        auto it = contentTypes.find(type);
        if (it == contentTypes.end()) return {};

        return it->second.extension;
    }
    string_view Response::ContentTypeToMimeType(ContentType type)
    {
        auto it = contentTypes.find(type);
        if (it == contentTypes.end()) return {};

        return it->second.mimeType;
    }

    ResponseType Response::StringToResponseType(string_view input)
    {
        for (const auto& [k, v] : statusLines)
        {
            if (v == input) return k;
        }

        return ResponseType::R_INVALID;
    }

    string_view Response::ResponseTypeToString(ResponseType type)
    {
        auto it = statusLines.find(type);
        if (it == statusLines.end()) return {};

        return it->second;
    }
}