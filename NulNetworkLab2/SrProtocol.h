#pragma once
#include "UdpReliableProtocol.h"

class SrProtocol : public UdpReliableProtocol {
public:
	SrProtocol(WSAConnection wsaConnection);
	
	virtual void response(const Socket& socket, const Socket::Address& target, const std::string& data) override;
	virtual std::string receive(const std::string& host, unsigned short port, double loss, double ackLoss) override;
};
