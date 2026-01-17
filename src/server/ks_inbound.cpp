//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#include <thread>

#include "KalaHeaders/thread_utils.hpp"

#include "server/ks_inbound.hpp"
#include "server/ks_server.hpp"

using KalaHeaders::KalaThread::lockwait_m;
using KalaHeaders::KalaThread::unlock_m;

using KalaServer::Server::ServerCore;
using KalaServer::Server::Connection;

using std::memory_order_acquire;
using std::memory_order_release;

static void _HandleWebRequest(Connection* c);

namespace KalaServer::Server
{
	void Inbound::HandleWebRequest(unique_ptr<Connection> c, bool isLocal)
	{
		Connection* l = c.get();

		//first pass in case request is local
		_HandleWebRequest(l);

		if (!isLocal)
		{
			l->isRunning.store(true, memory_order_release);

			l->connectionThread = thread([l]
				{
					while (ServerCore::IsListenerRunning()
						   && l->isRunning.load(memory_order_acquire))
					{
						_HandleWebRequest(l);
					}
				});

			lockwait_m(ServerCore::GetConnectMutex());
			ServerCore::GetConnectSockets().push_back(move(c));
			unlock_m(ServerCore::GetConnectMutex());
		}
	}
}

void _HandleWebRequest(Connection* c)
{

}