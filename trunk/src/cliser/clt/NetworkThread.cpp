// (c) Ivan Gagis
// e-mail: igagis@gmail.com
// Version: 1

// Description:
//          Network Thread class

#include <ting/Thread.hpp>
#include <ting/math.hpp>
#include <ting/Socket.hpp>
#include <ting/utils.hpp>
#include <ting/Ptr.hpp>

#include "NetworkThread.hpp"

using namespace ting;
using namespace cliser;



//override
void NetworkThread::Run(){
	while(!this->quitFlag){
		//queue is added to waitset, so, wait infinitely
		this->waitSet.Wait();

		if(this->queue.CanRead()){
			while(ting::Ptr<ting::Message> m = this->queue.PeekMsg()){
				//TRACE(<< "NetworkThread::Run(): message got, id = " << (m->ID()) << std::endl)
				m->Handle();
			}
		}

		if(this->socket.CanRead()){
			this->HandleSocketActivity();
		}
	}

//	TRACE(<< "NetworkThread::Run(): main loop exited" << std::endl)
	
	if(this->socket.IsValid()){
//		TRACE(<< "NetworkThread::Run(): socket is valid" << std::endl)
		this->waitSet.Remove(&this->socket);
//		TRACE(<< "NetworkThread::Run(): socket removed from thread" << std::endl)
		this->socket.Close();
//		TRACE(<< "NetworkThread::Run(): socket has been closed" << std::endl)
	}

	TRACE(<<"NetworkThread::Run(): exit"<<std::endl)
}



void NetworkThread::HandleSocketActivity(){
//    TRACE(<<"C_NetworkThread::HandleSocketActivity(): enter"<<std::endl)
	ASSERT(this->socket.IsValid())
	ASSERT(this->socket.CanRead())

	class ResListener : public cliser::NetworkReceiverState::PacketListener{
		NetworkThread* thr;

		//override
		void OnNewDataPacketReceived(ting::Array<ting::byte> d){
			this->thr->OnDataReceived(d);
		}

	public:
		ResListener(NetworkThread* p) :
				thr(p)
		{}
	} rl(this);

	if(this->netReceiverState.ReadSocket(&this->socket, &rl)){
		this->HandleDisconnection();
	}
}



void NetworkThread::HandleDisconnection(){
	this->waitSet.Remove(&this->socket);
	this->socket.Close();

	//notify about disconnection
	this->OnDisconnect();
}



void NetworkThread::Connect_ts(const std::string& host, ting::u16 port){
	//send connect request to thread
	this->PushMessage(
			ting::Ptr<ting::Message>(
					new ConnectToServerMessage(
							this,
							host,
							port
						)
				)
		);
}



void ConnectToServerMessage::Handle(){
	if(this->nt->socket.IsValid()){
		//already connected, notify OnConnected(failed)
		this->nt->OnConnect(NetworkThread::ALREADY_CONNECTED);
		return;
	}

	ting::IPAddress ip;

	try{
		ting::IPAddress tryParseIP(this->host.c_str(), this->port);
		ip = tryParseIP;
	}catch(ting::Socket::Exc& e){
		//ignore
	}

	//if given string is not an IP, try resolving host IP via DNS
	if(ip.host == 0){
		try{
			TRACE(<<"ConnectToServerMessage::Handle(): getting host by name" << std::endl)
			ip = ting::SocketLib::Inst().GetHostByName(this->host.c_str(), this->port);
			TRACE(<<"ConnectToServerMessage::Handle(): host by name got" << std::endl)
		}catch(ting::Socket::Exc& e){
			this->nt->OnConnect(NetworkThread::COULD_NOT_RESOLVE_HOST_IP);
			return;
		}
	}

//	TRACE(<<"ConnectToServerMessage::Handle(): host=" << reinterpret_cast<void*>(ip.host) << " port=" << (ip.port) << std::endl)
	try{
		TRACE(<<"ConnectToServerMessage::Handle(): connecting to host" << std::endl)
		this->nt->socket.Open(ip);
		TRACE(<<"ConnectToServerMessage::Handle(): connected to host" << std::endl)
	}catch(ting::Socket::Exc &e){
		TRACE(<<"ConnectToServerMessage::Handle(): exception caught, e = " << e.What() << ", sending connect failed reply to main thread" << std::endl)
		this->nt->OnConnect(NetworkThread::SOME_ERROR);
		return;
	}
	this->nt->waitSet.Add(&this->nt->socket, ting::Waitable::READ);

	this->nt->OnConnect(NetworkThread::SUCCESS);
}



void NetworkThread::SendData_ts(ting::Array<ting::byte> data){
	this->PushMessage(
			ting::Ptr<ting::Message>(
					new SendNetworkDataToServerMessage(this, data)
				)
		);
}



void SendNetworkDataToServerMessage::Handle(){
	if(!this->nt->socket.IsValid()){
		TRACE(<<"SendNetworkDataToServerMessage::Handle(): socket is disconnected, ignoring message"<<std::endl)
		return;
	}

	ASSERT(this->data.SizeInBytes() != 0 && this->data.SizeInBytes() <= (0xffff) )

	//send packet size
	ting::byte packetSize[2];
	ting::ToNetworkFormat16(this->data.SizeInBytes(), packetSize);

//	TRACE(<<"SendNetworkDataToServerMessage::Handle(): sending " << this->data.SizeInBytes() << " bytes" << std::endl)
//	TRACE(<<"SendNetworkDataToServerMessage::Handle(): bytes = " << u32(this->data[0]) << " " << "..." << std::endl)

	try{
		//send packet size
		this->nt->socket.SendAll(packetSize, sizeof(packetSize));

		//send data
		this->nt->socket.SendAll(this->data.Buf(), this->data.SizeInBytes());
	}catch(ting::Socket::Exc&){
		this->nt->HandleDisconnection();
		return;
	}
}



void NetworkThread::Disconnect_ts(){
	this->PushMessage(Ptr<Message>(
			new DisconnectFromServerMessage(this)
		));
}



//override
void DisconnectFromServerMessage::Handle(){
	this->nt->HandleDisconnection();
}
