// (c) Ivan Gagis
// e-mail: igagis@gmail.com
// Version: 1

// Description:
//          Thread class

#include <ting/Socket.hpp>
#include <ting/WaitSet.hpp>


#include "Server.hpp"
#include "TCPAcceptorThread.hpp"


using namespace cliser;



void TCPAcceptorThread::Run(){
	TRACE(<<"TCPAcceptorThread::Run(): enter"<<std::endl)

	ting::TCPServerSocket sock(this->port);//create and open listening socket

	ting::WaitSet waitSet(2);
	waitSet.Add(&sock, ting::Waitable::READ);
	waitSet.Add(&this->queue, ting::Waitable::READ);

	while(!this->quitFlag){
		waitSet.Wait();

		//TRACE(<<"C_TCPAcceptorThread::Run(): going to get message"<<std::endl)
		if(this->queue.CanRead()){
			while(ting::Ptr<ting::Message> m = this->queue.PeekMsg()){
				m->Handle();
			}
		}

		if(sock.CanRead()){
			ting::TCPSocket newSock;
			while((newSock = sock.Accept()).IsValid()){
				ting::Ptr<ting::Message> msg(
						new NewConnectionAcceptedMessage(this->serverMainThread, newSock)
					);
				this->serverMainThread->PushMessage(msg);
			}
		}
		//TRACE(<<"C_TCPAcceptorThread::Run(): cycle"<<std::endl)
	}

	TRACE(<<"C_TCPAcceptorThread::Run(): exit"<<std::endl)

};
