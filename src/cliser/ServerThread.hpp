/**
 * @file ServerThread.hpp
 * @author Ivan Gagis <igagis@gmail.com>
 * @brief Server thread which manages network connections on server side.
 * TODO: write long description
 */
#pragma once

#include <list>

#include <ting/debug.hpp>
#include <ting/types.hpp>
#include <ting/mt/MsgThread.hpp>
#include <ting/net/TCPSocket.hpp>

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
class ServerThread : public ting::mt::MsgThread{
	
	class ThreadsKillerThread : public ting::mt::MsgThread{
	public:
		ThreadsKillerThread(){};

		void Run()override;
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

	//override
	void Run();

private:
	ServerConnectionsThread* GetNotFullThread();



private:
	void HandleNewConnection(ting::net::TCPSocket socket);

	void HandleConnectionRemovedMessage(ServerThread::ServerConnectionsThread* cht);




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
		
		~ServerConnectionsThread()NOEXCEPT{}

		virtual std::shared_ptr<cliser::Connection> CreateConnectionObject()override{
			//this function will not be ever called.
			ASSERT(false);
			return std::shared_ptr<cliser::Connection>();
		}

		void OnConnected_ts(const std::shared_ptr<Connection>& c)override{
			ASS(this->serverThread)->listener->OnConnected_ts(c);
		}


		void OnDisconnected_ts(const std::shared_ptr<Connection>& c)override{
			//Disconnection may be because thread is exiting because server main thread
			//is also exiting, in that case no need to notify server main thread.
			//Send notification message to server main thread only if thread is not exiting yet.
			if(!this->quitFlag){
				this->serverThread->PushMessage(std::bind(
						[](ServerThread* serverMainThread, ServerThread::ServerConnectionsThread* clientsHandlerThread){
							serverMainThread->HandleConnectionRemovedMessage(clientsHandlerThread);
						},
						this->serverThread,
						this
					));
			}

			ASS(this->serverThread)->listener->OnDisconnected_ts(c);
		}

		bool OnDataReceived_ts(const std::shared_ptr<Connection>& c, const ting::Buffer<std::uint8_t> d)override{
			return ASS(this->serverThread)->listener->OnDataReceived_ts(c, d);
		}

		void OnDataSent_ts(const std::shared_ptr<Connection>& c, unsigned numPacketsInQueue, bool addedToQueue)override{
			ASS(this->serverThread)->listener->OnDataSent_ts(c, numPacketsInQueue, addedToQueue);
		}

		static std::unique_ptr<ServerConnectionsThread> New(
				ServerThread* serverThread,
				unsigned maxConnections
			)
		{
			return std::unique_ptr<ServerConnectionsThread>(
					new ServerConnectionsThread(
							serverThread,
							maxConnections
						)
				);
		}
	};
};



}//~namespace
