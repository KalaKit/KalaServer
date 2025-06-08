//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <unordered_map>
#include <string_view>
#include <array>
#include <algorithm>

namespace KalaKit::Core
{
	enum class MimeType
	{
		// SCRIPTS
		type_html, type_css, type_js,

		// IMAGES
		type_png, type_jpeg, type_svg_xml, type_gif, type_webp, type_ico,

		// AUDIO

		type_mp3, type_m4a, type_aac, type_ogg, type_opus, type_flac, type_wav,

		// VIDEO

		type_mp4, type_webm, type_mkv, type_mov, type_avi,

		// FONTS
		type_woff, type_woff2, type_ttf, type_otf,

		// DOCUMENTS AND TEXT
		type_plain, type_json, type_xml, type_csv, type_markdown,

		// ARCHIVES AND BINARIES
		type_zip, type_gz, type_tar, type_7z, type_rar, type_pdf, type_wasm
	};
}

namespace std
{
	template<>
	struct hash<KalaKit::Core::MimeType>
	{
		size_t operator()(const KalaKit::Core::MimeType& k) const noexcept
		{
			return static_cast<size_t>(k);
		}
	};
}

namespace KalaKit::Core
{
	using std::string_view;
	using std::unordered_map;
	using std::transform;
	using std::array;

	struct MimeEntry
	{
		MimeType type;
		string_view extension; //including the dot, e.g., ".html"
		string_view mime;
	};

	static inline const array<MimeEntry, 38> mimeTable = 
	{ {
		// SCRIPTS

		{ MimeType::type_html,     ".html", "text/html" },
		{ MimeType::type_css,      ".css",  "text/css" },
		{ MimeType::type_js,       ".js",   "application/javascript" },

		// IMAGES

		{ MimeType::type_png,       ".png",   "image/png" },
		{ MimeType::type_jpeg,      ".jpeg",  "image/jpeg" },
		{ MimeType::type_jpeg,      ".jpg",   "image/jpeg" },
		{ MimeType::type_gif,       ".gif",   "image/gif" },
		{ MimeType::type_webp,      ".webp",  "image/webp" },
		{ MimeType::type_ico,       ".ico",   "image/x-icon" },
		{ MimeType::type_svg_xml,   ".svg",   "image/svg+xml" },

		// AUDIO

		{ MimeType::type_mp3,   ".mp3",   "audio/mpeg" },
		{ MimeType::type_m4a,   ".m4a",   "audio/mp4" },
		{ MimeType::type_aac,   ".aac",   "audio/aac" },
		{ MimeType::type_ogg,   ".ogg",   "audio/ogg" },
		{ MimeType::type_opus,  ".opus",  "audio/opus" },
		{ MimeType::type_flac,  ".flac",  "audio/flac" },
		{ MimeType::type_wav,   ".wav",   "audio/wav" },

		// VIDEO

		{ MimeType::type_mp4,    ".mp4",   "video/mp4" },
		{ MimeType::type_webm,   ".webm",  "video/webm" },
		{ MimeType::type_mkv,    ".mkv",   "video/x-matroska" },
		{ MimeType::type_mov,    ".mov",   "video/quicktime" },
		{ MimeType::type_avi,    ".avi",   "video/x-msvideo" },

		// FONTS

		{ MimeType::type_woff,     ".woff", "font/woff" },
		{ MimeType::type_woff2,    ".woff2","font/woff2" },
		{ MimeType::type_ttf,      ".ttf",  "font/ttf" },
		{ MimeType::type_otf,      ".otf",  "font/otf" },

		// DOCUMENTS AND TEXT

		{ MimeType::type_plain,    ".txt",  "text/plain" },
		{ MimeType::type_json,     ".json", "application/json" },
		{ MimeType::type_xml,      ".xml",  "application/xml" },
		{ MimeType::type_csv,      ".csv",  "text/csv" },
		{ MimeType::type_markdown, ".md",   "text/markdown" },

		// ARCHIVES AND BINARIES

		{ MimeType::type_zip,      ".zip",  "application/zip" },
		{ MimeType::type_gz,       ".gz",   "application/gzip" },
		{ MimeType::type_tar,      ".tar",  "application/x-tar" },
		{ MimeType::type_7z,       ".7z",   "application/x-7z-compressed" },
		{ MimeType::type_rar,      ".rar",  "application/vnd.rar" },
		{ MimeType::type_pdf,      ".pdf",  "application/pdf" },
		{ MimeType::type_wasm,     ".wasm", "application/wasm" }
	} };

	static inline const auto ExtToTypeMap = []()
	{
		unordered_map<string_view, MimeType> map;
		for (const auto& entry : mimeTable)
		{
			map[entry.extension] = entry.type;
		}
		return map;
	}();

	static inline const auto TypeToMimeMap = []()
	{
		unordered_map<MimeType, string_view> map;
		for (const auto& entry : mimeTable)
		{
			if (!map.contains(entry.type))
			{
				map[entry.type] = entry.mime;
			}
		}
		return map;
	}();

	static string_view GetMimeType(const string_view& typeStr)
	{
		if (typeStr.empty()) return TypeToMimeMap.at(MimeType::type_html);

		string ext(typeStr);
		transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
		if (ext[0] != '.') ext = "." + ext;

		auto it = ExtToTypeMap.find(ext);
		if (it != ExtToTypeMap.end())
		{
			return TypeToMimeMap.at(it->second);
		}
		return TypeToMimeMap.at(MimeType::type_html);
	}
}