#pragma once
#include <stdexcept>

class NulException : public std::runtime_error {
public:
	NulException(int code, const std::string& message) : std::runtime_error(message), code(code) {}

	virtual int getCode() const {
		return this->code;
	}

protected:
	int code;
};
