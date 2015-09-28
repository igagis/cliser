/**
 * @file ClientThread.hpp
 * @author Ivan Gagis <igagis@gmail.com>
 * @brief Client thread which manages network connections on client side.
 * TODO: write long description
 */

#pragma once


#include <utki/Buf.hpp>
#include <utki/debug.hpp>
#include <setka/IPAddress.hpp>
#include <pogodi/WaitSet.hpp>

#include "ConnectionsThread.hpp"



//forward declarations
//...



namespace cliser{


/**
 * @brief Thread which handles network connections on client side.
 */
class ClientThread : public cliser::ConnectionsThread{
	friend class ConnectToServerMessage;


public:
	/**
	 * @brief Constructor.
	 * @param maxConnections - muximum number of connections this thread can handle.
	 * @param listener - pointer to listener object. Note, that it does not take ownership of the object.
	 */
	ClientThread(unsigned maxConnections, cliser::Listener* listener);

	virtual ~ClientThread()NOEXCEPT;

	/**
	 * @brief Request connection.
	 * @param ip - the IP address to connect to.
	 * @return - a newly created connection object. Initially it is not connected, as the connection
	 *           is an asynchronous operation. Completion of the connection operation is
	 *           notified through cliser::Listener::OnConnected_ts() method in case of
	 *           successful connection of through cliser::Listener::OnDisconnected_ts() in case
	 *           of unsuccessful result.
	 */
	//send connection request message to the thread
	std::shared_ptr<cliser::Connection> Connect_ts(const ting::net::IPAddress& ip);

private:
	void HandleConnectRequest(const ting::net::IPAddress& ip, const std::shared_ptr<cliser::Connection>& conn);
};



}//~namespace
