#pragma once
#include <string>

class WSAConnection final {
public:
	WSAConnection();
	~WSAConnection();

	std::string getDescription() const;
};
