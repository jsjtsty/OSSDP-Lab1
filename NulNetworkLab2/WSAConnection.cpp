#include "stdafx.h"
#include "sock.h"
#include "WSAConnection.h"
#include "NulWSAConnectionException.h"
#include <cstdint>
#include <mutex>

namespace {
	uint32_t instanceCount = 0;
	std::mutex mutex;
	WSAData wsaData;
}

WSAConnection::WSAConnection() {
	std::lock_guard<std::mutex> locked(mutex);

	if (instanceCount == 0) {
		int ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (ret != 0) {
			std::string message;
			switch (ret) {
			case WSASYSNOTREADY:
				message = "System not ready.";
				break;
			case WSAVERNOTSUPPORTED:
				message = "Version of Winsock 2.2 is not supported in this system.";
				break;
			case WSAEINPROGRESS:
				message = "Please restart the connection.";
				break;
			case WSAEPROCLIM:
				message = "Count of socket connections have reached the limit.";
				break;
			default:
				message = "Unknown Error";
				break;
			}
			throw NulWSAConnectionException(ret, message);
		}
	}
}

WSAConnection::~WSAConnection() {
	std::lock_guard<std::mutex> locked(mutex);

	--instanceCount;
	if (instanceCount == 0) {
		WSACleanup();
	}
}

std::string WSAConnection::getDescription() const {
	return wsaData.szDescription;
}
