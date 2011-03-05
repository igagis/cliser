// (c) Ivan Gagis
// e-mail: igagis@gmail.com

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
class ServerThread;



class ConnectionsThread : public ting::MsgThread{
	friend class ServerThread;
	friend class ClientThread;
	friend class Connection;

	typedef std::list<ting::Ref<Connection> > T_ConnectionsList;
	typedef T_ConnectionsList::iterator T_ConnectionsIter;
	T_ConnectionsList connections;
	ting::WaitSet waitSet;

private:
	ConnectionsThread(unsigned maxConnections);

	//override
	void Run();

	void HandleSocketActivity(ting::Ref<Connection>& conn);

public:
	~ConnectionsThread(){
		M_SRV_CLIENTS_HANDLER_TRACE(<< "ConnectionsThread::" << __func__ << "(): invoked" << std::endl)
		ASSERT(this->connections.size() == 0)
	}

	inline unsigned MaxConnections()const{
		return ASSCOND(this->waitSet.Size() - 1, > 0);
	}

private:
	virtual void OnConnected_ts(const ting::Ref<Connection>& c) = 0;

	virtual void OnDisconnected_ts(const ting::Ref<Connection>& c) = 0;

	virtual bool OnDataReceived_ts(const ting::Ref<Connection>& c, const ting::Buffer<ting::u8>& d) = 0;

	virtual void OnDataSent_ts(const ting::Ref<Connection>& c, unsigned numPacketsInQueue, bool addedToQueue){}

private:
	inline void AddSocketToSocketSet(
			ting::TCPSocket *sock,
			ting::Waitable::EReadinessFlags flagsToWaitFor = ting::Waitable::READ
		)
	{
		this->waitSet.Add(
				static_cast<ting::Waitable*>(sock),
				flagsToWaitFor
			);
	}

	inline void RemoveSocketFromSocketSet(ting::TCPSocket *sock){
		this->waitSet.Remove(sock);
	}



private:
	class AddConnectionMessage : public ting::Message{
		ConnectionsThread* thread;
		ting::Ref<Connection> conn;

	public:
		AddConnectionMessage(
				ConnectionsThread* t,
				ting::Ref<Connection>& c
			) :
				thread(ASS(t)),
				conn(ASS(c))
		{}

		//override
		void Handle(){
			ASS(this->thread)->HandleAddConnectionMessage(this->conn, true);
		}
	};

	void HandleAddConnectionMessage(const ting::Ref<Connection>& conn, bool isConnected);



	class RemoveConnectionMessage : public ting::Message{
		ConnectionsThread* thread;
		ting::Ref<Connection> conn;
	public:
		RemoveConnectionMessage(ConnectionsThread* t, ting::Ref<Connection>& c) :
				thread(ASS(t)),
				conn(ASS(c))
		{}

		//override
		void Handle(){
			this->thread->HandleRemoveConnectionMessage(this->conn);
		}
	};

	void HandleRemoveConnectionMessage(ting::Ref<Connection>& conn);



	class SendDataMessage : public ting::Message{
		ConnectionsThread *thread;//this mesage should hold reference to the thread this message is sent to

		ting::Ref<Connection> conn;

		ting::Array<ting::u8> data;

	  public:
		SendDataMessage(
				ConnectionsThread* clientThread,
				ting::Ref<Connection>& conn,
				ting::Array<ting::u8> d
			) :
				thread(ASS(clientThread)),
				conn(ASS(conn)),
				data(ASS(d))
		{}

		//override
		void Handle(){
			ASS(this->thread)->HandleSendDataMessage(this->conn, this->data);
		}
	};

	void HandleSendDataMessage(ting::Ref<Connection>& conn, ting::Array<ting::u8> data);



	class ResumeListeningForReadMessage : public ting::Message{
		ConnectionsThread* thread;
		ting::Ref<Connection> conn;
	public:
		ResumeListeningForReadMessage(ConnectionsThread* t, ting::Ref<Connection>& c) :
				thread(ASS(t)),
				conn(ASS(c))
		{}

		//override
		void Handle(){
			this->thread->HandleResumeListeningForReadMessage(this->conn);
		}
	};

	void HandleResumeListeningForReadMessage(ting::Ref<Connection>& conn);
};



}//~namespace
