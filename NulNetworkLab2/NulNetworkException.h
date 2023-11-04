#pragma once
#include "NulException.h"

class NulNetworkException : public NulException {
public:
	NulNetworkException(int code, const std::string& message) : NulException(code, message) {}
};
