//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#pragma once

#include <memory>

#include "KalaHeaders/core_utils.hpp"

namespace KalaServer::Server
{
	using std::unique_ptr;

	struct Connection;

	class LIB_API Inbound
	{
	public:
		//The purpose of this is to be a callback for Server::CreateListenerSocket so that
		//it handles each complete connection as a web request, great for websites.
		//Connection is a non-owning pointer that contains the socket and thread with mutex and empty ip for now.
		//IsLocal means this thread will stay alive after its first loop is done, otherwise it gets destroyed.
		static void HandleWebRequest(unique_ptr<Connection> c, bool isLocal);
	};
}