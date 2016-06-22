#include "sio_client.h"

#include <mutex>
#include <condition_variable>
#include <future>
#include <thread>
#include <windows.h>
#include <climits>
#include <vector>

namespace skt
{
	static unsigned long ID = 0;

	struct request
	{
		unsigned long id;
		bool isWaitingForAck;
		std::string tag;
		sio::message::ptr data;

		request(const std::string _tag, const sio::message::ptr _data, const bool _isWaitingForAck)
		{
			if (ID == LONG_MAX)
				ID = 0;
			id = ID++;
			isWaitingForAck = _isWaitingForAck;
			tag = _tag;
			data = _data;
		}

		~request(){};
	};


	class connection {
	public:
		
		connection()
		{
			isAcknowledged = false;
			isConnected = false;
			isTimeout = false;
			asyncTask = std::thread([&]()
			{
				connection::main();
			});
		};
		~connection() 
		{
			asyncTask.detach();
		};

		/*
		*	In the Async mode a callback function is used to collect data sent from the server,
		*	it's called at each response received from the corresponding connection
		*
		*@param ip (in)
		*	IP address
		*@param port (in)
		*	Port connection
		*@param f (out)
		*	Callback funtion, copy, paste and use this function in your code :
		*	void result(string const& tag, message::ptr const& data){}
		*@param timeout (in)
		*	(Optionnal) You can set the time out (ms) used to wait for the ack from the server, default 5000ms
		*
		*@return
		*	0 if successful
		*	1 if not connected
		*	2 if timed out
		*	3 if already connected
		*/
		int AsyncConnection(const std::string ip, const long port, void(*f)(std::string const& tag, sio::message::ptr const& data), const long timeout = 5000);


		/*
		*	In the Sync mode, results provided by the server are stored in a variable passed by reference
		*
		*@param ip (in)
		*	IP address
		*@param port (in)
		*	Port connection
		*@param f (out)
		*	Store all the responses sent from the server
		*@param timeout (in)
		*	(Optionnal) You can set the time out (ms) used to wait for the ack from the server
		*	Default 5000ms
		*
		*@return
		*	0 if successful
		*	1 if not connected
		*	2 if timed out
		*	3 if already connected
		*/
		int SyncConnection(const std::string ip, const long port, std::vector<std::pair<std::string, sio::message::ptr>> &result, const long timeout = 5000);
		

		/*
		*	Add a request to the stack, they are then sequencially sent to the server and saved in case of failure
		*
		*@param tag (in)
		*	Used as request identification from the server, make sure to code the response on server side accordingly
		*@param data in)
		*	Data to be transmitted to the server
		*@param isWaitForAck (in)
		*	(Optionnal) Define whether you want to wait for an acknowledgement of your request from the server
		*	In case of disconnection, if you add a request with a tag already stored in the stack and isWaitingForAck is set to false,
		*	the last request will be overwrited by the new one
		*	However, if isWaitingForAck is set to true, your request will be added in the stack 
		*	Default true
		*
		*@return
		*	0 if successful
		*	1 if disconnected
		*/
		int addRequest(const std::string tag, const sio::message::ptr data, const bool isWaitingForAck = true);

		/*
		*	Return the current number of pending request from the stack
		*
		*@return
		*	Return result
		*/
		int getPendingRequest(void);
		 
		///*
		//*@param tag
		//*	It is used as request identification from the server, make sure to code the response on server side accordingly
		//*@param data
		//*	Data to be transmitted to the server
		//*@param isWaitForAck
		//*	Define whether you want to wait for acknowledgement from the server
		//*
		//*@return
		//*	0 if successful
		//*	1 if disconnected
		//*	2 if timed out
		//*/
		//int SendMessageToServer(const std::string tag, const sio::message::ptr data, const bool isWaitForAck = false);

	private:
		std::vector<request> requests;
		std::vector<std::pair<std::string, sio::message::ptr>> *saveResult;
		void(*returnData)(std::string const& tag, sio::message::ptr const& data);
		sio::client currentClient;
		sio::socket::ptr currentSocket;
		std::mutex lock;
		std::mutex securedRequests;
		std::condition_variable_any cond;
		long timeout;
		bool isConnected;
		bool isTimeout;
		bool isAcknowledged;
		std::thread asyncTask;

		void main(void);
		int sendMessage(void);
		int find(const std::string tag);
	};
}
