#pragma once
#include "NulNetworkException.h"

class NulWSAConnectionException : public NulNetworkException {
public:
	NulWSAConnectionException(int code, const std::string& message) : NulNetworkException(code, message) {}
};

// Test B1.