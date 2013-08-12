/* The MIT License:

Copyright (c) 2009-2013 Ivan Gagis <igagis@gmail.com>

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

#include <ting/math.hpp>
#include <ting/util.hpp>
#include <ting/Ptr.hpp>
#include <ting/Buffer.hpp>
#include <ting/net/Lib.hpp>

#include "ClientThread.hpp"


using namespace cliser;



ClientThread::ClientThread(unsigned maxConnections, cliser::Listener* listener) :
		ConnectionsThread(maxConnections, listener)
{
	ASSERT(ting::net::Lib::IsCreated())
}



ClientThread::~ClientThread()throw(){
}



ting::Ref<cliser::Connection> ClientThread::Connect_ts(const ting::net::IPAddress& ip){
//    TRACE(<< "ClientThread::" << __func__ << "(): enter" << std::endl)

	ting::Ref<cliser::Connection> conn = ASS(this->listener)->CreateConnectionObject();
	//send connect request to thread
	this->PushMessage(
			ting::Ptr<ting::mt::Message>(
					new ConnectToServerMessage(
							this,
							ip,
							conn
						)
				)
		);
	return conn;
}



void ClientThread::HandleConnectRequest(
		const ting::net::IPAddress& ip,
		const ting::Ref<cliser::Connection>& conn
	)
{
//    TRACE(<< "ConnectToServerMessage::" << __func__ << "(): enter" << std::endl)
	ASSERT(conn)
	try{
		ASSERT(conn->socket.IsNotValid())
		conn->socket.Open(ip);
	}catch(ting::net::Exc &e){
//		TRACE(<< "ConnectToServerMessage::" << __func__ << "(): exception caught, e = " << e.What() << ", sending connect failed reply to main thread" << std::endl)
		ASS(this->listener)->OnDisconnected_ts(conn);
		return;
	}

	this->HandleAddConnectionMessage(conn, false);
}
