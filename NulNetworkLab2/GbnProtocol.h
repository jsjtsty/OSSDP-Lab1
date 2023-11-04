#pragma once
#include "UdpReliableProtocol.h"
#include <string>

class GbnProtocol : public UdpReliableProtocol {
public:
	GbnProtocol(WSAConnection wsaConnection);

	virtual void response(const Socket& socket, const Socket::Address& target, const std::string& data) override;
	virtual std::string receive(const std::string& host, unsigned short port, double loss, double ackLoss) override;
};

