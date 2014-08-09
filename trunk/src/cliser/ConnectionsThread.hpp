/* The MIT License:

Copyright (c) 2009-2014 Ivan Gagis <igagis@gmail.com>

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



#ifndef M_DOXYGEN_DONT_EXTRACT

#pragma once

#include <list>

#include <ting/net/TCPSocket.hpp>
#include <ting/WaitSet.hpp>
#include <ting/mt/MsgThread.hpp>

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
class ClientThread;



class ConnectionsThread : public ting::mt::MsgThread{
	friend class cliser::ServerThread;
	friend class cliser::ClientThread;
	friend class cliser::Connection;

	typedef std::list<std::shared_ptr<cliser::Connection> > T_ConnectionsList;
	typedef T_ConnectionsList::iterator T_ConnectionsIter;
	T_ConnectionsList connections;
	ting::WaitSet waitSet;

protected:
	cliser::Listener* const listener;
private:
	ConnectionsThread(unsigned maxConnections, cliser::Listener* listener);

	//override
	void Run();

	void HandleSocketActivity(std::shared_ptr<cliser::Connection>& conn);

public:
	~ConnectionsThread()throw();

	unsigned MaxConnections()const{
		return ASSCOND(this->waitSet.Size() - 1, > 0);
	}

private:
	void AddSocketToSocketSet(
			ting::net::TCPSocket &sock,
			ting::Waitable::EReadinessFlags flagsToWaitFor = ting::Waitable::READ
		)
	{
		this->waitSet.Add(
				sock,
				flagsToWaitFor
			);
	}

	inline void RemoveSocketFromSocketSet(ting::net::TCPSocket &sock){
		this->waitSet.Remove(sock);
	}



private:
	void HandleAddConnectionMessage(const std::shared_ptr<Connection>& conn, bool isConnected);


	void HandleRemoveConnectionMessage(std::shared_ptr<Connection>& conn);


	void HandleSendDataMessage(std::shared_ptr<Connection>& conn, std::shared_ptr<const SharedBuffer>&& data);

	void HandleResumeListeningForReadMessage(std::shared_ptr<Connection>& conn);
};//~class



}//~namespace

#endif //~doxygen
