/**
 * @file ServerThread.hpp
 * @author Ivan Gagis <igagis@gmail.com>
 * @brief Server thread which manages network connections on server side.
 * TODO: write long description
 */
#pragma once

#include <list>

#include <utki/debug.hpp>
#include <utki/Unique.hpp>
#include <nitki/MsgThread.hpp>
#include <setka/TCPSocket.hpp>

#include "ConnectionsThread.hpp"
#include "Connection.hpp"



namespace cliser{



/**
 * @brief Thread which handles network connections on server side.
 * This is a main thread of the connections server. It listens for new connections
 * and as new connection is accepted it moves this connection to another
 * thread which will handle the connection from now on. Thus, the server
 * distributes the connections between many handling threads. Each such handling
 * thread can handle up to a given maximum number of connections, this number is
 * determined by a constructor parameter (See constructor description).
 */
class ServerThread : public nitki::MsgThread, public utki::Unique{
	
	class ThreadsKillerThread : public nitki::MsgThread{
	public:
		ThreadsKillerThread(){}

		void run()override;
	} threadsKillerThread;

	//forward declaration
	class ServerConnectionsThread;

	std::list<std::unique_ptr<ServerConnectionsThread>> clientsThreads;

	std::uint16_t port;

	unsigned maxClientsPerThread;

	cliser::Listener* const listener;

	bool disableNaggle;
	
	std::uint16_t queueLength;
	
public:
	/**
	 * @brief Get maximum number of connections per thread.
	 * Get maximum number of connections per thread, it just returns a stored
	 * value of a corresponding number passed as a constructor parameter when
	 * creating the object.
     * @return maximum number of connections per thread.
     */
	unsigned MaxClientsPerThread()const{
		return this->maxClientsPerThread;
	}

	/**
	 * @brief Constructor.
     * @param port - TCP port to listen on for new connections.
     * @param maxClientsPerThread - maximum number of connections handled by a single handling thread.
     * @param listener - listener object to get notifications on network events.
     * @param disableNaggle - disable naggle algorithm for all connections.
     * @param acceptQueueLength - length of a queue of connection requests on the accepting network socket.
     */
	ServerThread(
			std::uint16_t port,
			unsigned maxClientsPerThread,
			cliser::Listener* listener,
			bool disableNaggle = false,
			std::uint16_t acceptQueueLength = 50
		);
	
	virtual ~ServerThread()throw();

	void run()override;

private:
	ServerConnectionsThread* getNotFullThread();



private:
	void handleNewConnection(setka::TCPSocket socket);

	void handleConnectionRemovedMessage(ServerThread::ServerConnectionsThread* cht);




private:
	class ServerConnectionsThread : private cliser::Listener, public ConnectionsThread{
		ServerThread* const serverThread;
	public:
		//This data is controlled by ServerThread
		unsigned numConnections = 0;
		//~

		ServerConnectionsThread(
				ServerThread* serverThread,
				unsigned maxConnections
			) :
				cliser::Listener(),
				ConnectionsThread(maxConnections, this),
				serverThread(ASS(serverThread))
		{}
		
		~ServerConnectionsThread()noexcept{}

		virtual std::shared_ptr<cliser::Connection> createConnectionObject()override{
			//this function will not be ever called.
			ASSERT(false);
			return std::shared_ptr<cliser::Connection>();
		}

		void onConnected_ts(const std::shared_ptr<Connection>& c)override{
			ASS(this->serverThread)->listener->onConnected_ts(c);
		}


		void onDisconnected_ts(const std::shared_ptr<Connection>& c)override{
			//Disconnection may be because thread is exiting because server main thread
			//is also exiting, in that case no need to notify server main thread.
			//Send notification message to server main thread only if thread is not exiting yet.
			if(!this->quitFlag){
				this->serverThread->pushMessage(std::bind(
						[](ServerThread* serverMainThread, ServerThread::ServerConnectionsThread* clientsHandlerThread){
							serverMainThread->handleConnectionRemovedMessage(clientsHandlerThread);
						},
						this->serverThread,
						this
					));
			}

			ASS(this->serverThread)->listener->onDisconnected_ts(c);
		}

		bool onDataReceived_ts(const std::shared_ptr<Connection>& c, const utki::Buf<std::uint8_t> d)override{
			return ASS(this->serverThread)->listener->onDataReceived_ts(c, d);
		}

		void onDataSent_ts(const std::shared_ptr<Connection>& c, unsigned numPacketsInQueue, bool addedToQueue)override{
			ASS(this->serverThread)->listener->onDataSent_ts(c, numPacketsInQueue, addedToQueue);
		}
	};
};



}//~namespace
