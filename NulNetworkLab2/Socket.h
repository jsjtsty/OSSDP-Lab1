#pragma once
#include <string>
#include <functional>
#include "WSAConnection.h"

class Socket final {
public:
	Socket(const Socket&) = delete;
	Socket(Socket&& other) noexcept;
	Socket(WSAConnection wsaConnection);
	~Socket();

	enum class IPType {
		IPv4,
		IPv6
	};

	enum class ProtocolType {
		TCP,
		UDP
	};

	class Address final {
	public:
		Address();
		Address(const std::string& host, unsigned short port);
		Address(const Address& other);
		Address(Address&& other) noexcept;
		~Address();

		void set(const std::string& host, unsigned short port);

		Address& operator=(Address&& other) noexcept;
		Address& operator=(Address& other);

		IPType getIpType() const;
		std::string getIp() const;
		unsigned short getPort() const;

	private:
		void* sockAddr;
		friend class Socket;
	};

	void init(ProtocolType protocolType, IPType ipType = IPType::IPv4);
	void bind(const std::string& ip, unsigned short port);
	void connect(const std::string& host, unsigned short port = 80U) const;
	void listen();
	Socket accept();
	Socket accept(Address& clientAddress);
	int receive(void* data, int length) const;
	int receive(void* data, int length, Address& sender) const;
	void send(const void* data, int length) const;
	void send(const std::string& data) const;
	void send(const void* data, int length, const Address& target) const;
	void send(const std::string& data, const Address& target) const;
	void close();

	void setBlockMode(bool blocked);
	WSAConnection getWsaConnection() const;

private:
	void* socket;
	IPType ipType;
	WSAConnection wsaConnection;
};

