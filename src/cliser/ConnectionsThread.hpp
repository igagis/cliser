#ifndef M_DOXYGEN_DONT_EXTRACT

#pragma once

#include <list>

#include <setka/TCPSocket.hpp>
#include <pogodi/WaitSet.hpp>
#include <nitki/MsgThread.hpp>

#include "Connection.hpp"



//#define M_ENABLE_SRV_CLIENTS_HANDLER_TRACE
#ifdef M_ENABLE_SRV_CLIENTS_HANDLER_TRACE
#define M_SRV_CLIENTS_HANDLER_TRACE(x) TRACE(x)
#else
#define M_SRV_CLIENTS_HANDLER_TRACE(x)
#endif


namespace cliser{



//forward declarations
class ServerThread;
class ClientThread;



class ConnectionsThread : public ting::mt::MsgThread{
	friend class cliser::ServerThread;
	friend class cliser::ClientThread;
	friend class cliser::Connection;

	typedef std::list<std::shared_ptr<cliser::Connection> > T_ConnectionsList;
	typedef T_ConnectionsList::iterator T_ConnectionsIter;
	T_ConnectionsList connections;
	ting::WaitSet waitSet;

protected:
	cliser::Listener* const listener;
private:
	ConnectionsThread(unsigned maxConnections, cliser::Listener* listener);

	//override
	void Run();

	void HandleSocketActivity(std::shared_ptr<cliser::Connection>& conn);

public:
	~ConnectionsThread()throw();

	unsigned MaxConnections()const{
		return ASSCOND(this->waitSet.Size() - 1, > 0);
	}

private:
	void AddSocketToSocketSet(
			ting::net::TCPSocket &sock,
			ting::Waitable::EReadinessFlags flagsToWaitFor = ting::Waitable::READ
		)
	{
		this->waitSet.Add(
				sock,
				flagsToWaitFor
			);
	}

	inline void RemoveSocketFromSocketSet(ting::net::TCPSocket &sock){
		this->waitSet.Remove(sock);
	}



private:
	void HandleAddConnectionMessage(const std::shared_ptr<Connection>& conn, bool isConnected);


	void HandleRemoveConnectionMessage(std::shared_ptr<Connection>& conn);


	void HandleSendDataMessage(std::shared_ptr<Connection>& conn, std::shared_ptr<const std::vector<std::uint8_t>>&& data);

	void HandleResumeListeningForReadMessage(std::shared_ptr<Connection>& conn);
};//~class



}//~namespace

#endif //~doxygen
