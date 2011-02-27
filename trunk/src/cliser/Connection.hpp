// (c) Ivan Gagis
// e-mail: igagis@gmail.com
// Version: 1

// Description:
//          Client class

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



class Connection : public ting::RefCounted{
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
	ting::Mutex mutex;

	ting::Array<ting::u8> receivedData;//Should be protected with mutex

	inline void SetHandlingThread(ConnectionsThread *thr){
		ASSERT(thr)
		ting::Mutex::Guard mutexGuard(this->mutex);
		//Assert that client is not added to some thread already.
		ASSERT_INFO(!this->parentThread, "client's handler thread is already set")
		this->parentThread = thr;
	}

	inline void ClearHandlingThread(){
		ting::Mutex::Guard mutexGuard(this->mutex);
		this->parentThread = 0;
	}

protected:

public:
	inline Connection() :
			parentThread(0)
	{}

	virtual ~Connection(){
//		TRACE(<< "~cliser::Client(): invoked" << std::endl)
	}

	void Disconnect_ts();

	void Send_ts(ting::Array<ting::u8> data);
	void SendCopy_ts(const ting::Buffer<ting::u8>& data);

	ting::Array<ting::u8> GetReceivedData_ts();

	static inline ting::Ref<cliser::Connection> New(){
		return ting::Ref<cliser::Connection>(ASS(new cliser::Connection()));
	}
};

}//~namespace
