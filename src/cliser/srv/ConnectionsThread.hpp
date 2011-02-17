// (c) Ivan Gagis
// e-mail: igagis@gmail.com
// Version: 1

// Description:
//          Clients Handler Thread class

#pragma once

#include <list>

#include <ting/Socket.hpp>
#include <ting/types.hpp>
#include <ting/Array.hpp>
#include <ting/Ref.hpp>
#include <ting/WaitSet.hpp>

#include "Connection.hpp"



//#define M_ENABLE_SRV_CLIENTS_HANDLER_TRACE
#ifdef M_ENABLE_SRV_CLIENTS_HANDLER_TRACE
#define M_SRV_CLIENTS_HANDLER_TRACE(x) TRACE(x)
#else
#define M_SRV_CLIENTS_HANDLER_TRACE(x)
#endif


namespace cliser{



//forward declarations
class Server;



class ConnectionsThread : public ting::MsgThread{
	friend class Connection;

	Server* const smt;

	typedef std::list<ting::Ref<Connection> > T_ConnectionsList;
	typedef T_ConnectionsList::iterator T_ConnectionsIter;
	T_ConnectionsList connections;
	ting::WaitSet waitSet;

	//This data is controlled by Server Main Thread
	unsigned numClients;
	//~
public:

	ConnectionsThread(Server *serverMainThread);

	~ConnectionsThread(){
		M_SRV_CLIENTS_HANDLER_TRACE(<< "~TCPClientsHandlerThread(): invoked" << std::endl)
		this->waitSet.Remove(&this->queue);
		ASSERT(this->connections.size() == 0)
	}

	//override
	void Run();

private:
	inline void AddSocketToSocketSet(ting::TCPSocket *sock){
		this->waitSet.Add(
				static_cast<ting::Waitable*>(sock),
				ting::Waitable::READ
			);
	}

	inline void RemoveSocketFromSocketSet(ting::TCPSocket *sock){
		this->waitSet.Remove(sock);
	}

	//TODO: remove
//	void HandleSocketActivities();

	//TODO: remove
	//return true if client has beed disconnected
//	bool HandleClientSocketActivity(ting::Ref<Connection>& c);



private:
	class AddConnectionMessage : public ting::Message{
		ConnectionsThread* thread;
		ting::Ref<Connection> conn;
	public:


		AddConnectionMessage(
				ConnectionsThread* t,
				ting::Ref<Connection>& c
			) :
				thread(t),
				conn(c)
		{
			ASSERT(this->thread)
			ASSERT(this->conn)
		}

		//override
		void Handle();
	};


	
	class RemoveConnectionMessage : public ting::Message{
		ConnectionsThread* thread;
		ting::Ref<Connection> conn;
	  public:
		RemoveConnectionMessage(ConnectionsThread* t, ting::Ref<Connection>& c) :
				thread(t),
				conn(c)
		{
			ASSERT(this->thread)
			ASSERT(this->conn)
		}

		//override
		void Handle();
	};



	class SendDataMessage : public ting::Message{
		ConnectionsThread *cht;//this mesage should hold reference to the thread this message is sent to

		ting::Ref<Connection> conn;

		ting::Array<ting::u8> data;

	  public:
		SendDataMessage(
				ConnectionsThread* clientThread,
				ting::Ref<Connection>& clt,
				ting::Array<ting::u8> d
			) :
				cht(clientThread),
				conn(clt),
				data(d)
		{
			ASSERT(this->cht)
			ASSERT(this->conn)
			ASSERT(this->data.Size() != 0)
		}

		//override
		void Handle();
	};

};



}//~namespace
