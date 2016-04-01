#ifndef M_DOXYGEN_DONT_EXTRACT

#pragma once

#include <list>

#include <setka/TCPSocket.hpp>
#include <pogodi/WaitSet.hpp>
#include <nitki/MsgThread.hpp>
#include <utki/Unique.hpp>

#include "Connection.hpp"



//#define M_ENABLE_SRV_CLIENTS_HANDLER_TRACE
#ifdef M_ENABLE_SRV_CLIENTS_HANDLER_TRACE
#	define M_SRV_CLIENTS_HANDLER_TRACE(x) TRACE(x)
#else
#	define M_SRV_CLIENTS_HANDLER_TRACE(x)
#endif


namespace cliser{



//forward declarations
class ServerThread;
class ClientThread;



class ConnectionsThread : public nitki::MsgThread, public utki::Unique{
	friend class cliser::ServerThread;
	friend class cliser::ClientThread;
	friend class cliser::Connection;

	typedef std::list<std::shared_ptr<cliser::Connection> > T_ConnectionsList;
	typedef T_ConnectionsList::iterator T_ConnectionsIter;
	T_ConnectionsList connections;
	pogodi::WaitSet waitSet;

protected:
	cliser::Listener* const listener;
private:
	ConnectionsThread(unsigned maxConnections, cliser::Listener* listener);

	void run()override;

	void handleSocketActivity(std::shared_ptr<cliser::Connection>& conn);

public:
	~ConnectionsThread()throw();

	unsigned maxConnections()const{
		return ASSCOND(this->waitSet.size() - 1, > 0);
	}

private:
	void addSocketToSocketSet(
			setka::TCPSocket &sock,
			pogodi::Waitable::EReadinessFlags flagsToWaitFor = pogodi::Waitable::READ
		)
	{
		this->waitSet.add(
				sock,
				flagsToWaitFor
			);
	}

	inline void removeSocketFromSocketSet(setka::TCPSocket &sock){
		this->waitSet.remove(sock);
	}



private:
	void handleAddConnectionMessage(const std::shared_ptr<Connection>& conn, bool isConnected);


	void handleRemoveConnectionMessage(std::shared_ptr<Connection>& conn);


	void handleSendDataMessage(std::shared_ptr<Connection>& conn, std::shared_ptr<const std::vector<std::uint8_t>>&& data);

	void handleResumeListeningForReadMessage(std::shared_ptr<Connection>& conn);
};//~class



}//~namespace

#endif //~doxygen
