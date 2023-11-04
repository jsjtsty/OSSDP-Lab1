#include "stdafx.h"
#include "UdpReliableServer.h"
#include "util.h"
#include <iostream>
#include <fstream>

// Test modify 1.0.

int main() {
	WSAConnection wsaConnection;
	UdpReliableServer server(wsaConnection);
	std::ofstream logger;
	logger.open("log.log", std::ios::ate);
	server.setLogger([&](std::string message) {
		logger << message << std::endl;
		logger.flush();
	});

	std::string ip, portString;
	unsigned short port;

	std::cout << "NulNetworkLab2 - GBN and SR protocols" << std::endl << 
		"Copyright (C) NulStudio 2014-2023. Licensed under GPLv3." << std::endl << std::endl;

	std::cout << "# Initialize Protocol: " << std::endl;
	std::cout << "Input [Listen IP]: ";
	std::getline(std::cin, ip);
	std::cout << "Input [Listen Port]: ";
	std::getline(std::cin, portString);
	port = std::stoi(portString);
	portString.clear();

	server.init(ip, port);
	server.start();

	std::cout << std::endl << "# Terminal: " << std::endl;

	std::string targetHost = ip;
	unsigned short targetPort = port;

	while (true) {
		std::cout << ">>> ";
		std::string inst;
		std::getline(std::cin, inst);
		std::vector<std::string> instList = util::split(inst, " ");

		for (auto iter = instList.begin(); iter != instList.end(); ++iter) {
			*iter = util::trim(*iter);
			if (iter->empty()) {
				iter = instList.erase(iter);
			}
			if (iter == instList.end()) {
				break;
			}
		}

		if (instList.size() == 0) {
			std::cout << "Invalid instruction, please try again." << std::endl;
			continue;
		}

		const std::string& inst0 = instList[0];
		if (inst0 == "-settarget") {
			if (instList.size() < 3) {
				std::cout << "Invalid instruction, please try again." << std::endl;
				continue;
			}
			targetHost = instList[1];
			targetPort = (unsigned short) std::stoi(instList[2]);
			std::cout << "Successfully set host and port." << std::endl;
		} else if (inst0 == "-testgbn") {
			double loss = 0.2 , ackLoss = 0.2;
			if (instList.size() >= 2) {
				loss = std::stod(instList[1]);
			} 
			if (instList.size() >= 3) {
				ackLoss = std::stod(instList[2]);
			}
			std::string result = server.sendTestRequest(targetHost, targetPort, UdpReliableServer::ProtocolType::GBN, loss, ackLoss);
			std::cout << result << std::endl;
		} else if (inst0 == "-testsr") {
			double loss = 0.2, ackLoss = 0.2;
			if (instList.size() >= 2) {
				loss = std::stod(instList[1]);
			}
			if (instList.size() >= 3) {
				ackLoss = std::stod(instList[2]);
			}
			std::string result = server.sendTestRequest(targetHost, targetPort, UdpReliableServer::ProtocolType::SR, loss, ackLoss);
			std::cout << result << std::endl;
		} else {
			std::string result = server.send(targetHost, targetPort, inst);
			std::cout << result << std::endl;
		}
	}

	server.close();
	return 0;
}
