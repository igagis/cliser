/* The MIT License:

Copyright (c) 2009-2012 Ivan Gagis <igagis@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE. */

//Home page: http://code.google.com/p/cliser/

/**
 * @file Connection.hpp
 * @author Ivan Gagis <igagis@gmail.com>
 * @brief Connection base class.
 * Connection base class. User is supposed to derive his own connection
 * object from it.
 */

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



/**
 * @brief Connection base class.
 * This class incapsulates all the data used by the cliser library to receive and send data.
 * Basically, it incapsulates the network socket and some other data.
 * Also, user can send data to remote end through a Connection object, using its Send_ts() and SendCopy_ts() methods.
 * User should subcluss the cliser::Connection object to include his own data.
 * Connection objects are created using abstract factory method of cliser::Listener interface.
 */
class Connection : public virtual ting::RefCounted{
	friend class ConnectionsThread;
	friend class ServerThread;
	friend class ClientThread;

	//This is the network data associated with Connection
	std::list<ting::Array<ting::u8> > packetQueue;
	unsigned dataSent;//number of bytes sent from first packet in the queue
	//~

	ting::net::TCPSocket socket;
	ting::Waitable::EReadinessFlags currentFlags;

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
	inline Connection() :
			parentThread(0)
	{}

public:
	virtual ~Connection()throw(){
//		TRACE(<< "Connection::" << __func__ << "(): invoked" << std::endl)
	}

	/**
	 * @brief Request disconnection.
	 * This method sends a disconnect request. As a result the
	 * cliser::Listener::OnDisconnected_ts() method will be called, this will
	 * indicate that the disconnection has actually happened.
	 * This method is thread safe (the _ts suffix tells that).
	 */
	void Disconnect_ts();

	/**
	 * @brief Request data sending to the remote end.
	 * This method requests data sending to the remote end.
	 * The cliser::Listener::OnDataSent_ts() method will be called to indicate
	 * the operation completion. Note, that possibly all the data cannot be sent at once,
	 * in that case the data is added to sending queue and will be sent when possible.
	 * When data is added to send queue the cliser::Listener::OnDataSent_ts() is also called
	 * with its arguments appropriately set, indicating that the data was not sent, but adde to
	 * sending queue.
	 * @param data - data to send.
	 */
	void Send_ts(ting::Array<ting::u8> data);

	/**
	 * @brief Request sending of data copy to the remote end.
	 * This methods works same as Send_ts() except that it copies the data passed
	 * as argument instead of taking ownership of the passed ting::Array.
	 * @param data
	 */
	void SendCopy_ts(const ting::Buffer<ting::u8>& data);

	/**
	 * @brief Get stored received data and resume listening for incoming data.
	 * This method gets stored received data if any, and resumes the listening for incoming data.
	 * The arrival of network data from the remote end is notified through
	 * cliser::Listener::OnDataReceived_ts() callback method and the user is
	 * supposed to handle the data and return true value from the method. But,
	 * if it is not possible to handle the data at current point of time the user
	 * can return false from the callback method and then the received data will be
	 * stored inside the cliser::Connection object and the listening for incoming
	 * data will be stopped for this connection. Thus, when the user is able to handle the data
	 * he should call the GetReceivedData_ts() method to get the data, and resume listening for
	 * further incoming data from the remote end.
	 * @return the valid Array with the data if there is data stored.
	 * @return invalid Array object if there is no data stored.
	 */
	ting::Array<ting::u8> GetReceivedData_ts();
};



/**
 * @brief Abstract interface to get notifications on network events.
 * This is an abstract interface which is supposed to be implemented by user.
 * The interface is used to get notifications on network events and handle them.
 * Also it is used to construct Connection objects, it has the
 * CreateConnectionObject() factory method for that.
 * Note, that methods marked with _ts suffix are suposed to be thread safe. That
 * means that the methods will be called from different threads and user should do
 * any synchronization which is needed when implementing these methods.
 */
class Listener{

	//NOTE: this numTimesAdded variable is only needed to check that
	//      the listener object has not been deleted before its owner
	//      (the object which does notifications through this listener).
	friend class cliser::ConnectionsThread;
	friend class ServerThread;
	ting::Inited<unsigned, 0> numTimesAdded;

public:

	/**
	 * @brief Create connection object.
	 * This method is called when cliser system needs a new connection object.
	 * The user is supposed to reimplement the method and return a reference to
	 * the newly created Connection object.
	 * @return ting::Ref reference to the newly created Connection object.
	 */
	virtual ting::Ref<cliser::Connection> CreateConnectionObject() = 0;

	/**
	 * @brief New connection estabilished.
	 * This callback method is called by cliser system to indicate that new
	 * connection was estabilished.
	 * The method is supposed to be thread safe.
	 * @param c - connection object of the new connection.
	 */
	virtual void OnConnected_ts(const ting::Ref<Connection>& c) = 0;

	/**
	 * @brief connection closed.
	 * This callback method is called by cliser system to indicate that a
	 * connection was closed.
	 * The method is supposed to be thread safe.
	 * @param c - connection object of the connection which was closed.
	 */
	virtual void OnDisconnected_ts(const ting::Ref<Connection>& c) = 0;

	/**
	 * @brief Data received.
	 * This callback method is called by cliser system to indicate that
	 * some network data was received from remote end.
	 * The method is supposed to be thread safe.
	 * @param c - connection object.
	 * @param d -data which was received.
	 * @return true if the data was successfully handled.
	 * @return false if the data was not handled. In that case, the data will be stored in the
	 *         Connection object. Listening for new incoming data from remote end will also be stopped.
	 *         The data can later be retrieved using cliser::Connection::GetReceivedData_ts()
	 *         method which will also resume listening for new incoming data from remote end.
	 */
	virtual bool OnDataReceived_ts(const ting::Ref<Connection>& c, const ting::Buffer<ting::u8>& d) = 0;

	/**
	 * @brief Data has been sent.
	 * This callback method is called by cliser system to indicate that the data was
	 * sent or placed to sending queue. Note, that for data placed into the sending queue
	 * this method will be called twice: first, when the data is placed to the queue and
	 * second time, when the data has actually been sent to the remote end.
	 * The method is supposed to be thread safe.
     * @param c - Connection object.
     * @param numPacketsInQueue - number of packets currently stored in the sending queue of the connection object.
     * @param addedToQueue - flag indicating that the data was stored to the sending queue instead of actually sending.
     */
	virtual void OnDataSent_ts(const ting::Ref<Connection>& c, unsigned numPacketsInQueue, bool addedToQueue){}

	virtual ~Listener(){
		ASSERT(this->numTimesAdded == 0)
	}
};



}//~namespace
