#include "stdafx.h"
#include "UdpReliableProtocol.h"
#include <mutex>

namespace {
	std::mutex mutex;
}

UdpReliableProtocol::UdpReliableProtocol(WSAConnection wsaConnection) 
	: logger([](std::string) {}), wsaConnection(wsaConnection) {}

void UdpReliableProtocol::setLogger(Logger logger, bool locked) {
	if (locked) {
		this->logger = [logger](std::string message) {
			std::lock_guard<std::mutex> locked(mutex);
			logger(message);
		};
	} else {
		this->logger = logger;
	}
}
