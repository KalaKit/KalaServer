//Copyright(C) 2025 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include "response/response.hpp"

namespace KalaKit::ResponseSystem
{
	class Response_200 : public Response
	{
	public:
		void Init(
			uintptr_t targetClientSocket,
			const string& targetClientIP,
			const string& targetRoute,
			const string& targetContentType) override;
	};
}