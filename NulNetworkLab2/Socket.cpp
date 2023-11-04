#include "stdafx.h"
#include "sock.h"
#include "Socket.h"
#include "NulNetworkException.h"
#include <format>

// M2

typedef Socket::IPType IPType;

namespace {
	inline SOCKET& GetSocket(void* socket) {
		return *((SOCKET*)socket);
	}
}

Socket::Socket(Socket&& other) noexcept {
	this->socket = other.socket;
	this->ipType = std::move(other.ipType);
	this->wsaConnection = std::move(other.wsaConnection);
	other.socket = nullptr;
}

Socket::Socket(WSAConnection wsaConnection) : socket(new SOCKET(INVALID_SOCKET)),
ipType(IPType::IPv4), wsaConnection(wsaConnection) {}

Socket::~Socket() {
	this->close();
	delete this->socket;
}

void Socket::init(ProtocolType protocolType, IPType ipType) {
	int inetType = AF_INET, type = SOCK_STREAM, protocol = IPPROTO_TCP;

	switch (protocolType) {
	case ProtocolType::TCP:
		type = SOCK_STREAM;
		protocol = IPPROTO_TCP;
		break;
	case ProtocolType::UDP:
		type = SOCK_DGRAM;
		protocol = IPPROTO_UDP;
		break;
	}

	switch (ipType) {
	case IPType::IPv4:
		inetType = AF_INET;
		break;
	case IPType::IPv6:
		inetType = AF_INET6;
		break;
	}

	SOCKET& socket = GetSocket(this->socket);
	socket = ::socket(inetType, type, protocol);

	if (socket == INVALID_SOCKET) {
		throw NulNetworkException(WSAGetLastError(), "Failed to initialize socket.");
	}
}

void Socket::bind(const std::string& ip, unsigned short port) {
	sockaddr_in addr;
	switch (this->ipType) {
	case IPType::IPv4:
		addr.sin_family = AF_INET;
		inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
		break;
	case IPType::IPv6:
		addr.sin_family = AF_INET6;
		inet_pton(AF_INET6, ip.c_str(), &addr.sin_addr);
		break;
	}
	addr.sin_port = htons(port);

	SOCKET& socket = GetSocket(this->socket);

	int res = ::bind(socket, (SOCKADDR*)&addr, sizeof(SOCKADDR));
	if (res) {
		int errorCode = GetLastError();
		throw NulNetworkException(errorCode, std::format("Failed to bind port {}.", port));
	}
}

void Socket::connect(const std::string& host, unsigned short port) const {
	SOCKET& socket = GetSocket(this->socket);
	int res = 0;
	sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));

	int afType = AF_INET;
	switch (this->ipType) {
	case IPType::IPv4:
		afType = AF_INET;
		break;
	case IPType::IPv6:
		afType = AF_INET6;
		break;
	}

	addr.sin_family = afType;
	addr.sin_port = htons(port);

	addrinfo hints, *resultRaw;
	std::memset(&hints, 0, sizeof(hints));

	res = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &resultRaw);
	if (res != 0) {
		throw NulNetworkException(GetLastError(), std::format("Failed to resolve host {}", host));
	}

	auto addrInfoDeleter = [](addrinfo* ptr) {
		freeaddrinfo(ptr);
	};
	std::unique_ptr<addrinfo, decltype(addrInfoDeleter)> result(resultRaw, addrInfoDeleter);
	resultRaw = nullptr;

	for (addrinfo* resultPtr = result.get(); resultPtr != nullptr; resultPtr = resultPtr->ai_next) {
		res = ::connect(socket, resultPtr->ai_addr, (int)resultPtr->ai_addrlen);
		if (res == 0) {
			break;
		}
	}

	if (res != 0) {
		throw NulNetworkException(WSAGetLastError(), std::format("Failed to connect to host {}, port {}.", host, port));
	}
}

void Socket::listen() {
	SOCKET& socket = GetSocket(this->socket);
	if (socket == INVALID_SOCKET) {
		throw NulNetworkException(0, "Server not initialized.");
	}

	int res = ::listen(socket, SOMAXCONN);
	if (res == SOCKET_ERROR) {
		throw NulNetworkException(WSAGetLastError(), "Failed to listen port.");
	}
}

Socket Socket::accept() {
	SOCKET& serverSocket = GetSocket(this->socket);
	sockaddr_in acceptAddr;
	int addrLen = sizeof(SOCKADDR_IN);

	SOCKET clientSocket = ::accept(serverSocket, (sockaddr*)&acceptAddr, &addrLen);
	if (clientSocket == INVALID_SOCKET) {
		throw NulNetworkException(WSAGetLastError(), "Accept failed.");
	}

	Address address;
	memcpy(address.sockAddr, &acceptAddr, sizeof(sockaddr));
	Socket socketInstance(this->wsaConnection);
	memcpy(socketInstance.socket, &clientSocket, sizeof(SOCKET));
	socketInstance.ipType = address.getIpType();
	return socketInstance;
}

Socket Socket::accept(Address& clientAddress) {
	SOCKET& serverSocket = GetSocket(this->socket);
	sockaddr_in acceptAddr;
	int addrLen = sizeof(SOCKADDR_IN);

	SOCKET clientSocket = ::accept(serverSocket, (sockaddr*)&acceptAddr, &addrLen);
	if (clientSocket == INVALID_SOCKET) {
		throw NulNetworkException(WSAGetLastError(), "Accept failed.");
	}

	Address address;
	memcpy(address.sockAddr, &acceptAddr, sizeof(sockaddr));
	Socket socketInstance(this->wsaConnection);
	memcpy(socketInstance.socket, &clientSocket, sizeof(SOCKET));
	socketInstance.ipType = address.getIpType();
	clientAddress = std::move(address);
	return socketInstance;
}

void Socket::setBlockMode(bool blocked) {
	u_long mode = blocked ? 0 : 1;
	SOCKET& socket = GetSocket(this->socket);
	ioctlsocket(socket, FIONBIO, &mode);
}

WSAConnection Socket::getWsaConnection() const {
	return this->wsaConnection;
}

int Socket::receive(void* data, int length) const {
	SOCKET& socket = GetSocket(this->socket);
	if (socket == INVALID_SOCKET) {
		throw NulNetworkException(0, "Invalid socket.");
	}
	int res = ::recv(socket, (char*)data, length, 0);
	return res;
}

int Socket::receive(void* data, int length, Address& sender) const {
	SOCKET& socket = GetSocket(this->socket);
	if (socket == INVALID_SOCKET) {
		throw NulNetworkException(0, "Invalid socket.");
	}
	int addrLen = sizeof(sockaddr);
	int res = ::recvfrom(socket, (char*)data, length, 0, (sockaddr*)sender.sockAddr, &addrLen);
	return res;
}

void Socket::send(const void* data, int length) const {
	SOCKET& socket = GetSocket(this->socket);
	if (socket == INVALID_SOCKET) {
		throw NulNetworkException(0, "Invalid socket.");
	}
	int res = ::send(socket, (const char*)data, length, 0);
	if (res == SOCKET_ERROR) {
		throw NulNetworkException(WSAGetLastError(), "Failed to send message.");
	}
}

void Socket::send(const std::string& data) const {
	this->send(data.c_str(), (int)data.size());
}

void Socket::send(const void* data, int length, const Address& targetSocketInstance) const {
	SOCKET& socket = GetSocket(this->socket);
	if (socket == INVALID_SOCKET) {
		throw NulNetworkException(0, "Invalid socket.");
	}
	int res = ::sendto(socket, (const char*)data, length, 0, (sockaddr*)targetSocketInstance.sockAddr, sizeof(sockaddr));
	if (res == SOCKET_ERROR) {
		throw NulNetworkException(WSAGetLastError(), "Failed to send message.");
	}
}

void Socket::send(const std::string & data, const Address & target) const {
	this->send(data.c_str(), (int)data.size(), target);
}

void Socket::close() {
	if (this->socket == nullptr) {
		return;
	}
	SOCKET& socket = GetSocket(this->socket);
	if (socket != INVALID_SOCKET) {
		closesocket(socket);
		socket = INVALID_SOCKET;
	}
}

Socket::Address::Address() {
	this->sockAddr = new sockaddr;
	std::memset(this->sockAddr, 0, sizeof(sockaddr));
}

Socket::Address::Address(const std::string & host, unsigned short port) {
	this->sockAddr = new sockaddr;
	std::memset(this->sockAddr, 0, sizeof(sockaddr));
	this->set(host, port);
}

Socket::Address::Address(const Address& other) {
	this->sockAddr = new sockaddr;
	std::memcpy(this->sockAddr, other.sockAddr, sizeof(sockaddr));
}

Socket::Address::Address(Address&& other) noexcept {
	this->sockAddr = other.sockAddr;
	other.sockAddr = nullptr;
}

Socket::Address::~Address() {
	delete this->sockAddr;
}

void Socket::Address::set(const std::string & host, unsigned short port) {
	int res = 0;

	addrinfo hints, *resultRaw;
	std::memset(&hints, 0, sizeof(hints));

	res = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &resultRaw);
	if (res != 0) {
		throw NulNetworkException(GetLastError(), std::format("Failed to resolve host {}", host));
	}

	std::memcpy(this->sockAddr, resultRaw->ai_addr, sizeof(sockaddr));
	freeaddrinfo(resultRaw);
}

Socket::Address& Socket::Address::operator=(Address&& other) noexcept {
	this->sockAddr = other.sockAddr;
	other.sockAddr = nullptr;
	return *this;
}

Socket::Address& Socket::Address::operator=(Address& other) {
	memcpy(this->sockAddr, other.sockAddr, sizeof(sockaddr));
	return *this;
}

IPType Socket::Address::getIpType() const {
	IPType result = IPType::IPv4;
	switch (((sockaddr*)this->sockAddr)->sa_family) {
	case AF_INET:
		result = IPType::IPv4;
		break;
	case AF_INET6:
		result = IPType::IPv6;
		break;
	}
	return result;
}

std::string Socket::Address::getIp() const {
	sockaddr* addr = (sockaddr*)this->sockAddr;
	char* ip = nullptr;
	std::string result;
	switch (addr->sa_family) {
	case AF_INET:
		ip = new char[INET_ADDRSTRLEN];
		inet_ntop(addr->sa_family, &((sockaddr_in*)sockAddr)->sin_addr, ip, INET_ADDRSTRLEN);
		result = ip;
		break;
	case AF_INET6:
		ip = new char[INET6_ADDRSTRLEN];
		inet_ntop(addr->sa_family, &((sockaddr_in6*)sockAddr)->sin6_addr, ip, INET6_ADDRSTRLEN);
		result = ip;
		break;
	}
	delete[] ip;
	return result;
}

unsigned short Socket::Address::getPort() const {
	sockaddr* addr = (sockaddr*)this->sockAddr;
	unsigned short port = 0;
	switch (addr->sa_family) {
	case AF_INET:
		port = ntohs(((sockaddr_in*)addr)->sin_port);
		break;
	case AF_INET6:
		port = ntohs(((sockaddr_in6*)addr)->sin6_port);
		break;
	}
	return port;
}
