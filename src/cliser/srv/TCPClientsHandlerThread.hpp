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

#include "Client.hpp"
#include "Server.hpp"



//#define M_ENABLE_SRV_CLIENTS_HANDLER_TRACE
#ifdef M_ENABLE_SRV_CLIENTS_HANDLER_TRACE
#define M_SRV_CLIENTS_HANDLER_TRACE(x) TRACE(x)
#else
#define M_SRV_CLIENTS_HANDLER_TRACE(x)
#endif


namespace cliser{



//forward declarations
class Server;



class TCPClientsHandlerThread : public ting::MsgThread{
	friend class AddClientToThreadMessage;
	friend class RemoveClientFromThreadMessage;

	Server* const smt;

	typedef std::list<ting::Ref<Client> > T_ClientsList;
	typedef T_ClientsList::iterator T_ClientsIter;
	T_ClientsList clients;
	ting::WaitSet waitSet;

public:
	//This data is controlled by Server Main Thread
	unsigned numClients;
	//~

	TCPClientsHandlerThread(Server *serverMainThread);

	~TCPClientsHandlerThread(){
		M_SRV_CLIENTS_HANDLER_TRACE(<< "~TCPClientsHandlerThread(): invoked" << std::endl)
		this->waitSet.Remove(&this->queue);
		ASSERT(this->clients.size() == 0)
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

	void HandleSocketActivities();

	//return true if client has beed disconnected
	bool HandleClientSocketActivity(ting::Ref<Client>& c);
};



class AddClientToThreadMessage : public ting::Message{
	TCPClientsHandlerThread* thread;
	ting::Ref<Client> client;
	ting::TCPSocket socket;
public:


	AddClientToThreadMessage(
			TCPClientsHandlerThread* t,
			ting::Ref<Client>& c,
			ting::TCPSocket clientSocket
		) :
			thread(t),
			client(c),
			socket(clientSocket)
	{
		ASSERT(this->thread)
		ASSERT(this->client)
		ASSERT(this->socket.IsValid())
	}

	//override
	void Handle();
};



class RemoveClientFromThreadMessage : public ting::Message{
	TCPClientsHandlerThread* thread;
	ting::Ref<Client> client;
  public:
	RemoveClientFromThreadMessage(TCPClientsHandlerThread* t, ting::Ref<Client>& c) :
			thread(t),
			client(c)
	{
		ASSERT(this->thread)
		ASSERT(this->client)
	}

	//override
	void Handle();
};



class SendNetworkDataToClientMessage : public ting::Message{
	TCPClientsHandlerThread *cht;//this mesage should hold reference to the thread this message is sent to

	ting::Ref<Client> client;

	ting::Array<ting::u8> data;

  public:
	SendNetworkDataToClientMessage(
			TCPClientsHandlerThread* clientThread,
			ting::Ref<Client>& clt,
			ting::Array<ting::u8> d
		) :
			cht(clientThread),
			client(clt),
			data(d)
	{
		ASSERT(this->cht)
		ASSERT(this->client)
		ASSERT(this->data.Size() != 0)
	}

	//override
	void Handle();
};



}//~namespace
