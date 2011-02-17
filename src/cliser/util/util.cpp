// (c) Ivan Gagis
// e-mail: igagis@gmail.com
// Version: 1

// Description:
//

#include <ting/debug.hpp>
#include <ting/math.hpp>
#include <ting/utils.hpp>
#include <ting/Buffer.hpp>

#include "util.hpp"


using namespace cliser;
using namespace ting;


bool NetworkReceiverState::ReadSocket(
		ting::TCPSocket* s,
		NetworkReceiverState::PacketListener* rl
	)
{
	ASSERT(rl)

	//receive data from socket
	ting::StaticBuffer<ting::u8, 8192> data;//8kb
	unsigned numRecvd;
	try{
		numRecvd = s->Recv(data);
	}catch(Socket::Exc& e){
		TRACE_AND_LOG(<< "NetworkReceiverState::ReadSocket(): Recv() has thrown an exception: " << e.What() << std::endl)
		//some error, terminate connection
		return true;
	}
//	TRACE(<< "Recv " << numRecvd << " bytes" << std::endl)
	ASSERT(numRecvd <= data.Size())

	if(numRecvd == 0){//connection closed by peer
//		TRACE(<<"NetworkReceiverState::ReadSocket(): socket disconnected"<<std::endl)
		return true;//socket disconnected
	}

	ting::u8* curDataPtr = data.Buf();
	for(unsigned numBytesUnparsed = numRecvd; numBytesUnparsed > 0 ;){
		if(this->numBytesToReceive != 0){//we know the packet size and are in process of receiving packet
			ASSERT(this->receivedData.Size()!=0)
			ASSERT(this->numBytesToReceive <= this->receivedData.Size())

			unsigned numBytesToCopy = (std::min)(numBytesUnparsed, this->numBytesToReceive);
			memcpy(
					this->receivedData.Buf() + (this->receivedData.Size() - this->numBytesToReceive),
					curDataPtr,
					numBytesToCopy
				);
			this->numBytesToReceive -= numBytesToCopy;

			numBytesUnparsed -= numBytesToCopy;
			curDataPtr += numBytesToCopy;

			if(this->numBytesToReceive == 0){
				//packet received completely
//				TRACE(<<"NetworkReceiverState::ReadSocket(): packet received completely!!! Size = " << this->receivedData.SizeInBytes() << " bytes" << std::endl)
				//notify about new data received
				rl->OnNewDataPacketReceived(this->receivedData);
			}
		}else{//we must be receiving packet size
			//try to read size of the packet, we need this "for" because it is possible that
			//the number of bytes received is less than size of packetSize variable
			for(; this->numBytesInPacketSizeHolder < this->packetSizeHolder.SizeInBytes() &&
					numBytesUnparsed > 0;
				)
			{
				ASSERT(this->numBytesInPacketSizeHolder < this->packetSizeHolder.SizeInBytes())
				ting::u8 *p = &this->packetSizeHolder[0] + this->numBytesInPacketSizeHolder;
				*p = *curDataPtr;
				++(this->numBytesInPacketSizeHolder);

				--numBytesUnparsed;
				++curDataPtr;

				ASSERT(numBytesUnparsed <= numRecvd)
			}

			if(this->numBytesInPacketSizeHolder != this->packetSizeHolder.SizeInBytes()){
				ASSERT(numBytesUnparsed == 0)
				return false; //must be ran out of unparsed data
			}

			//if we get here then we have completely read packet size
			this->numBytesInPacketSizeHolder = 0;//reset bytes counter

			ting::u16 packetSize = ting::Deserialize16(&this->packetSizeHolder[0]);
			if(packetSize > 0){//if packet has nonzero size
				this->receivedData.Init(packetSize);
				this->numBytesToReceive = this->receivedData.SizeInBytes();
			}else{//packet has zero size
				continue;//read packet size again
			}
		}//~else
	}//~for(unparsedData)

	return false;
}
