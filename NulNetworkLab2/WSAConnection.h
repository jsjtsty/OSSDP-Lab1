#pragma once
#include <string>

// M2

class WSAConnection final {
public:
	WSAConnection();
	~WSAConnection();

	std::string getDescription() const;
};

// Test B1.