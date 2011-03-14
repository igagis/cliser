// (c) Ivan Gagis
// e-mail: igagis@gmail.com

#pragma once

#include <ting/types.hpp>
#include <ting/Array.hpp>
#include <ting/Buffer.hpp>
#include <ting/Ref.hpp>
#include <ting/Socket.hpp>
#include <ting/Thread.hpp>



namespace cliser{



//forward declarations
class ConnectionsThread;



class Connection : public virtual ting::RefCounted{
	friend class ConnectionsThread;
	friend class ServerThread;
	friend class ClientThread;

	//This is the network data associated with Connection
	std::list<ting::Array<ting::u8> > packetQueue;
	unsigned dataSent;//number of bytes sent from first packet in the queue
	//~

	ting::TCPSocket socket;
	
	//NOTE: clientThread may be accessed from different threads, therefore, protect it with mutex
	ConnectionsThread *parentThread;
	ting::Mutex parentThreadMutex;

	ting::Array<ting::u8> receivedData;//Should be protected with mutex

	inline void SetHandlingThread(ConnectionsThread *thr){
		ASSERT(thr)
		ting::Mutex::Guard mutexGuard(this->parentThreadMutex);
		//Assert that client is not added to some thread already.
		ASSERT_INFO(!this->parentThread, "client's handler thread is already set")
		this->parentThread = thr;
	}

	inline void ClearHandlingThread(){
		ting::Mutex::Guard mutexGuard(this->parentThreadMutex);
		this->parentThread = 0;
	}

protected:
	ting::Mutex receivedDataMutex;

	inline Connection() :
			parentThread(0)
	{}

public:
	virtual ~Connection(){
//		TRACE(<< "Connection::" << __func__ << "(): invoked" << std::endl)
	}

	void Disconnect_ts();

	void Send_ts(ting::Array<ting::u8> data);
	void SendCopy_ts(const ting::Buffer<ting::u8>& data);

	ting::Array<ting::u8> GetReceivedData_ts();
};



class Listener{

	//NOTE: this numTimesAdded variable is only needed to check that
	//      the listener object has not been deleted before its owner
	//      (the object which does notifications through this listener).
	friend class cliser::ConnectionsThread;
	friend class ServerThread;
	ting::Inited<unsigned, 0> numTimesAdded;

public:
	
	virtual ting::Ref<cliser::Connection> CreateConnectionObject() = 0;

	virtual void OnConnected_ts(const ting::Ref<Connection>& c) = 0;

	virtual void OnDisconnected_ts(const ting::Ref<Connection>& c) = 0;

	virtual bool OnDataReceived_ts(const ting::Ref<Connection>& c, const ting::Buffer<ting::u8>& d) = 0;

	virtual void OnDataSent_ts(const ting::Ref<Connection>& c, unsigned numPacketsInQueue, bool addedToQueue){}

	virtual ~Listener(){
		ASSERT(this->numTimesAdded == 0)
	}
};



}//~namespace
