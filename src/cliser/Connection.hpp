/**
 * @file Connection.hpp
 * @author Ivan Gagis <igagis@gmail.com>
 * @brief Connection base class.
 * Connection base class. User is supposed to derive his own connection
 * object from it.
 */

#pragma once

#include <utki/Buf.hpp>
#include <utki/Shared.hpp>

#include <setka/TCPSocket.hpp>

#include <list>
#include <vector>
#include <mutex>


namespace cliser{



//forward declarations
class ConnectionsThread;



/**
 * @brief Connection base class.
 * This class incapsulates all the data used by the cliser library to receive and send data.
 * Basically, it incapsulates the network socket and some other data.
 * Also, user can send data to remote end through a Connection object, using its Send_ts() and SendCopy_ts() methods.
 * User should subclass the cliser::Connection object to include his own data.
 * Connection objects are created using abstract factory method of cliser::Listener interface.
 */
class Connection : public virtual ting::Shared{
	friend class ConnectionsThread;
	friend class ServerThread;
	friend class ClientThread;

	//This is the network data associated with Connection
	std::list<std::shared_ptr<const std::vector<std::uint8_t>>> packetQueue;
	unsigned dataSent;//number of bytes sent from first packet in the queue
	//~

	ting::net::TCPSocket socket;
	ting::Waitable::EReadinessFlags currentFlags;

	//NOTE: clientThread may be accessed from different threads, therefore, protect it with mutex
	ConnectionsThread *parentThread = 0;
	std::mutex mutex;

	std::vector<std::uint8_t> receivedData;//Should be protected with mutex

	void SetHandlingThread(ConnectionsThread *thr){
		ASSERT(thr)
		std::lock_guard<decltype(this->mutex)> mutexGuard(this->mutex);
		//Assert that client is not added to some thread already.
		ASSERT_INFO(!this->parentThread, "client's handler thread is already set")
		this->parentThread = thr;
	}

	void ClearHandlingThread(){
		std::lock_guard<decltype(this->mutex)> mutexGuard(this->mutex);
		this->parentThread = 0;
	}

protected:
	Connection(){}

public:
	virtual ~Connection()NOEXCEPT{
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
	void Send_ts(const std::shared_ptr<const std::vector<std::uint8_t>>& data);

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
	 * @return non-empty vector with the data if there is data stored.
	 * @return empty vector if there is no data stored.
	 */
	const std::vector<std::uint8_t> GetReceivedData_ts();
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
	unsigned numTimesAdded = 0;

public:

	/**
	 * @brief Create connection object.
	 * This method is called when cliser system needs a new connection object.
	 * The user is supposed to reimplement the method and return a reference to
	 * the newly created Connection object.
	 * @return std::shared_ptr reference to the newly created Connection object.
	 */
	virtual std::shared_ptr<cliser::Connection> CreateConnectionObject() = 0;

	/**
	 * @brief New connection estabilished.
	 * This callback method is called by cliser system to indicate that new
	 * connection was estabilished.
	 * The method is supposed to be thread safe.
	 * @param c - connection object of the new connection.
	 */
	virtual void OnConnected_ts(const std::shared_ptr<Connection>& c) = 0;

	/**
	 * @brief connection closed.
	 * This callback method is called by cliser system to indicate that a
	 * connection was closed.
	 * The method is supposed to be thread safe.
	 * @param c - connection object of the connection which was closed.
	 */
	virtual void OnDisconnected_ts(const std::shared_ptr<Connection>& c) = 0;

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
	virtual bool OnDataReceived_ts(const std::shared_ptr<Connection>& c, const ting::Buffer<std::uint8_t> d) = 0;

	/**
	 * @brief Data has been sent.
	 * This callback method is called by cliser system to indicate that the data was
	 * sent or placed to sending queue. The data is either sent right a way or placed to sending queue,
	 * depending on load of network connection. Note, that for data placed into the sending queue
	 * this method will be called twice: first, when the data is placed to the queue and
	 * second time, when the data has actually been sent to the remote end.
	 * The method is supposed to be thread safe.
     * @param c - Connection object.
     * @param numPacketsInQueue - number of packets currently stored in the sending queue of the connection object.
     * @param addedToQueue - flag indicating that the data was stored to the sending queue instead of actually sending.
     */
	virtual void OnDataSent_ts(const std::shared_ptr<Connection>& c, unsigned numPacketsInQueue, bool addedToQueue){}

	virtual ~Listener()NOEXCEPT{
		ASSERT(this->numTimesAdded == 0)
	}
};



}//~namespace
