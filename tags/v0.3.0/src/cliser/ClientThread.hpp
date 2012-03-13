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
 * @file ClientThread.hpp
 * @author Ivan Gagis <igagis@gmail.com>
 * @brief Client thread which manages network connections on client side.
 * TODO: write long description
 */

#pragma once

#include <ting/Array.hpp>
#include <ting/Buffer.hpp>
#include <ting/types.hpp>
#include <ting/debug.hpp>
#include <ting/Thread.hpp>
#include <ting/net/IPAddress.hpp>
#include <ting/WaitSet.hpp>

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

	virtual ~ClientThread()throw();

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
	ting::Ref<cliser::Connection> Connect_ts(const ting::net::IPAddress& ip);

private:
	class ConnectToServerMessage : public ting::Message{
		ClientThread* ct;
		ting::net::IPAddress ip;
		const ting::Ref<cliser::Connection> conn;
	public:
		ConnectToServerMessage(
				ClientThread* ct,
				const ting::net::IPAddress& ip,
				const ting::Ref<cliser::Connection>& conn
			) :
				ct(ASS(ct)),
				ip(ip),
				conn(conn)
		{}

		//override
		void Handle(){
//			TRACE(<< "ConnectToServerMessage::" << __func__ << "(): host=" << reinterpret_cast<void*>(ip.host) << " port=" << (ip.port) << std::endl)
			this->ct->HandleConnectRequest(this->ip, this->conn);
		}
	};

	void HandleConnectRequest(const ting::net::IPAddress& ip, const ting::Ref<cliser::Connection>& conn);
};



}//~namespace
