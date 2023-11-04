#include "stdafx.h"
#include "UdpReliableServer.h"
#include <memory>
#include <thread>
#include <mutex>
#include <cstdint>
#include <map>
#include <random>
#include <fstream>
#include "util.h"
#include "NulNetworkException.h"
#include "GbnProtocol.h"
#include "SrProtocol.h"

constexpr int BUFFER_LENGTH = 1026;

namespace {
	std::mutex mutex;

	std::string ReadTestData() {
		std::ifstream ifs("test.txt");
		std::string result;
		int c;
		while ((c = ifs.get()) != EOF) {
			result += (char)c;
		}
		return result;
	}

	const std::map<std::string, std::function<std::string(const Socket&, const Socket::Address&,
		UdpReliableServer::Logger)>> serverInstructionMap = {
		{"-time", [](const Socket&, const Socket::Address&, UdpReliableServer::Logger) {
			return util::get_local_time_string("%Y.%m.%d %H:%M:%S");
		}},
		{"-quit", [](const Socket&, const Socket::Address&, UdpReliableServer::Logger) {
			return "Good bye!";
		}},
		{"-testgbn", [](const Socket& socket, const Socket::Address& target, UdpReliableServer::Logger logger) {
			GbnProtocol gbn(socket.getWsaConnection());
			gbn.setLogger(logger);
			gbn.response(socket, target, ReadTestData());
			return std::string();
		}},
		{"-testsr", [](const Socket& socket, const Socket::Address& target, UdpReliableServer::Logger logger) {
			SrProtocol sr(socket.getWsaConnection());
			sr.setLogger(logger);
			sr.response(socket, target, ReadTestData());
			return std::string();
		}}
	};
}

UdpReliableServer::UdpReliableServer(WSAConnection wsaConnection) 
	: wsaConnection(wsaConnection), socket(wsaConnection), logger([](std::string) {}), serverStarted(false) {}

void UdpReliableServer::init(const std::string& host, unsigned short port) {
	socket.init(Socket::ProtocolType::UDP);
	socket.bind(host, port);
	socket.setBlockMode(false);
}

void UdpReliableServer::start() {
	if (serverStarted) {
		return;
	}

	serverStarted = true;

	std::thread serverThread = std::thread([this]() {
		std::unique_ptr<uint8_t[]> buffer = std::make_unique<uint8_t[]>(BUFFER_LENGTH);
		Socket::Address sender;
		while (serverStarted) {
			// 非阻塞监听指定端口
			int res = socket.receive(buffer.get(), BUFFER_LENGTH, sender);
			if (res < 0) {
				continue;
			}

			// 处理指令，查表执行程序
			std::string instruction = util::trim(std::string(reinterpret_cast<const char*>(buffer.get()), res));
			std::string result;

			if (serverInstructionMap.contains(instruction)) {
				result = serverInstructionMap.at(instruction)(socket, sender, logger);
			} else {
				result = instruction;
			}

			// 不在表格中的指令直接返回
			if (!result.empty()) {
				socket.send(result, sender);
			}
		}
	});

	serverThread.detach();
}

void UdpReliableServer::close() {
	serverStarted = false;
}

std::string UdpReliableServer::sendTestRequest(const std::string& host, unsigned short port, ProtocolType protocolType, 
	double loss, double ackLoss) const {
	Socket socket(wsaConnection);
	socket.init(Socket::ProtocolType::UDP);
	Socket::Address target(host, port);
	std::string result;

	std::unique_ptr<UdpReliableProtocol> protocol;

	if (protocolType == ProtocolType::GBN) {
		protocol = std::make_unique<GbnProtocol>(wsaConnection);
	} else if (protocolType == ProtocolType::SR) {
		protocol = std::make_unique<SrProtocol>(wsaConnection);
	} else {
		throw NulNetworkException(0, "Invalid protocol type.");
	}

	protocol->setLogger(logger);
	return protocol->receive(host, port, loss, ackLoss);
}

std::string UdpReliableServer::send(const std::string& host, unsigned short port, const std::string& message) const {
	Socket socket(wsaConnection);
	Socket::Address target(host, port);
	std::unique_ptr<char[]> buffer = std::make_unique<char[]>(BUFFER_LENGTH);
	socket.init(Socket::ProtocolType::UDP);
	socket.send(message, target);
	int length = socket.receive(buffer.get(), BUFFER_LENGTH, target);

	std::string result;
	if (length > 0) {
		result.append(buffer.get(), length);
	}
	return result;
}

void UdpReliableServer::setLogger(Logger logger) {
	this->logger = [logger](std::string message) {
		std::lock_guard<std::mutex> locked(mutex);
		logger(message);
	};
}





