#include "socketIO.h"

namespace skt
{
	int connection::SyncConnection(const std::string ip, const long port, std::vector<std::pair<std::string, sio::message::ptr>> &result, const long time)
	{
		if (isConnected)
			return 3;

		timeout = time;
		isConnected = false;
		isTimeout = false;
		saveResult = &result;
		 
		currentClient.set_open_listener([&]()
		{
			cond.notify_all();
			isConnected = true;
		});

		new std::thread([&]()
		{
			Sleep(timeout);
			cond.notify_all();
			isTimeout = true;
		});

		std::string address = "http://" + ip + ":" + std::to_string(port);
		currentClient.connect(address);

		lock.lock();
		if (!isConnected && !isTimeout)
			cond.wait(lock); 
		lock.unlock(); 

		if (isConnected)
		{
			currentSocket = currentClient.socket();
			currentSocket->on("message", sio::socket::event_listener_aux([&](std::string const& name, sio::message::ptr const& data, bool isAck, sio::message::list &ack_resp)
			{
				std::string tag = data->get_map()["tag"]->get_string();
				sio::message::ptr myData = sio::object_message::create();
				myData = data->get_map()["myData"];
				saveResult->push_back(std::make_pair(tag, myData));
			}));
			return 0;
		}
		else if (isTimeout)
			return 2;

		return 1;
	}

	int connection::AsyncConnection(const std::string ip, const long port, void(*f)(std::string const& tag, sio::message::ptr const& data), const long time)
	{
		if (isConnected)
			return 3;

		timeout = time;
		isConnected = false;
		isTimeout = false;
		returnData = f;

		currentClient.set_open_listener([&]()
		{
			cond.notify_all();
			isConnected = true;
		});

		new std::thread([&]()
		{
			Sleep(timeout);
			cond.notify_all();
			isTimeout = true;
		});

		std::string address = "http://" + ip + ":" + std::to_string(port);
		currentClient.connect(address);

		lock.lock();
		if (!isConnected && !isTimeout)
			cond.wait(lock);
		lock.unlock();

		if (isConnected)
		{
			currentSocket = currentClient.socket();
			currentSocket->on("message", sio::socket::event_listener_aux([&](std::string const& name, sio::message::ptr const& data, bool isAck, sio::message::list &ack_resp)
			{
				std::string tag = data->get_map()["tag"]->get_string();
				sio::message::ptr myData = sio::object_message::create();
				myData = data->get_map()["myData"];
				returnData(tag, myData);
			}));
			return 0;
		}
		else if (isTimeout)
			return 2;

		return 1;
	}

	int connection::addRequest(const std::string tag, const sio::message::ptr data, const bool isWaitForAck)
	{
		securedRequests.lock();
		if (!isWaitForAck)
		{
			int tmp = find(tag);
			if (tmp != -1)
				requests.at(tmp) = request(tag, data, isWaitForAck);
			else
				requests.push_back(request(tag, data, isWaitForAck));
		}
		else
			requests.push_back(request(tag, data, isWaitForAck));
		securedRequests.unlock();

		if (!isConnected)
			return 1;

		return 0;
	}

	int connection::getPendingRequest(void)
	{
		return requests.size();
	}

	int connection::find(const std::string tag)
	{
		for (unsigned int i(0); i < requests.size(); i++)
		{
			if (!requests[i].tag.compare(tag))
				return i;
		}
		return -1;
	}

	void connection::main(void)
	{
		while (1)
		{
			Sleep(500);
			while (requests.size() && isConnected)
			{
				if (!currentClient.opened())
					isConnected = false;
				else if (!sendMessage())
				{
					securedRequests.lock();
					requests.erase(requests.begin());
					securedRequests.unlock();
				}
			}
		}
	}

	int connection::sendMessage(void)
	{
		isAcknowledged = false;
		isTimeout = false;

		if (!isConnected)
			return 1;

		securedRequests.lock();
		if (requests[0].isWaitingForAck)
		{
			currentSocket->emit(requests[0].tag, requests[0].data, [&](sio::message::list const& msg)
			{
				cond.notify_all();
				isAcknowledged = true;
			});
			securedRequests.unlock();

			new std::thread([&]()
			{
				Sleep(timeout);
				cond.notify_all();
				isTimeout = true;
			});

			lock.lock();
			if (!isAcknowledged && !isTimeout)
				cond.wait(lock);
			lock.unlock();

			if (isAcknowledged)
				return 0;
			else if (isTimeout)
				return 2;
		}
		else
		{
			currentSocket->emit(requests[0].tag, requests[0].data);
			securedRequests.unlock();
		}
		
		return 0;
	}

	//int connection::SendMessageToServer(const std::string tag, const sio::message::ptr data, const bool isWaitForAck)
	//{
	//	isAcknowledged = false;
	//	isTimeout = false;
	//
	//	if (!currentClient.opened())
	//		return 1;
	//	if (isWaitForAck)
	//	{
	//		currentSocket->emit(tag, data, [&](sio::message::list const& msg)
	//		{
	//			cond.notify_all();
	//			isAcknowledged = true;
	//		});
	//
	//		new std::thread([&]()
	//		{
	//			Sleep(timeout);
	//			cond.notify_all();
	//			isTimeout = true;
	//		});
	//
	//		lock.lock();
	//		if (!isAcknowledged && !isTimeout)
	//			cond.wait(lock);
	//		lock.unlock();
	//
	//		if (isAcknowledged)
	//			return 0;
	//		else if (isTimeout)
	//			return 2;
	//	}
	//	else
	//		currentSocket->emit(tag);
	//
	//	return 0;
	//}
}
