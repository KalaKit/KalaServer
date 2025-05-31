//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include "response/response.hpp"

namespace KalaKit::ResponseSystem
{
	class Response_OK : public Response
	{
	public:
		void Init(
			const std::string& route,
			const std::string& clientIP,
			uintptr_t clientSocket) override;
	};
}