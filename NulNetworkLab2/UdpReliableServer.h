#pragma once
#include "Socket.h"
#include <functional>
#include <string>
#include <atomic>

class UdpReliableServer final {
public:
	UdpReliableServer(WSAConnection wsaConnection);

	enum class ProtocolType {
		GBN,
		SR
	};

	typedef std::function<void(std::string)> Logger;

	void init(const std::string& host, unsigned short port);
	void start();
	void close();

	std::string sendTestRequest(const std::string& host, unsigned short port, ProtocolType protocolType, 
		double loss = 0.2, double ackLoss = 0.2) const;
	std::string send(const std::string& host, unsigned short port, const std::string& message) const;

	void setLogger(Logger logger);

private:
	WSAConnection wsaConnection;
	Socket socket;
	Logger logger;
	std::atomic_bool serverStarted;
};

