#pragma once
#include "Socket.h"

class UdpReliableProtocol {
public:
	UdpReliableProtocol(WSAConnection wsaConnection);
	virtual ~UdpReliableProtocol() = default;

	typedef std::function<void(std::string)> Logger;

	virtual void response(const Socket& socket, const Socket::Address& target, const std::string& data) = 0;
	virtual std::string receive(const std::string& host, unsigned short port, double loss, double ackLoss) = 0;

	void setLogger(Logger logger, bool locked = false);

protected:
	Logger logger;
	WSAConnection wsaConnection;
};

// Test B2.