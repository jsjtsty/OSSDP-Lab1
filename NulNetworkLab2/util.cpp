#include "stdafx.h"
#include <Windows.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>

namespace util {

	std::string trim(const std::string& val) {
		if (val.empty()) {
			return std::string();
		}

		size_t left = 0, right = val.size() - 1;
		for (; left < val.size(); ++left) {
			if (!isspace(val[left])) {
				break;
			}
		}
		for (; right > 0; --right) {
			if (!isspace(val[right])) {
				break;
			}
		}
		std::string result = val.substr(left, right - left + 1);
		return result;
	}

	std::string trim_left(const std::string& val) {
		if (val.empty()) {
			return std::string();
		}

		size_t left = 0;
		for (; left < val.size(); ++left) {
			if (!isspace(val[left])) {
				break;
			}
		}

		if (left == val.size()) {
			return std::string();
		}

		std::string result = val.substr(left, val.size() - left);
		return result;
	}

	std::string trim_right(const std::string& val) {
		if (val.empty()) {
			return std::string();
		}

		size_t right = val.size() - 1;
		for (; right > 0; --right) {
			if (!isspace(val[right])) {
				break;
			}
		}

		if (right == 0) {
			if (isspace(val[0])) {
				return std::string();
			}
		}

		std::string result = val.substr(0, right + 1);
		return result;
	}

	std::vector<std::string> split(const std::string& val, const std::string& splitor) {
		std::vector<std::string> result;

		if (splitor.empty()) {
			for (const char c : val) {
				result.push_back(std::string(1, c));
			}
		} else {
			size_t prev = 0, pos = 0;
			while (true) {
				pos = val.find(splitor, prev);
				if (pos == std::string::npos) {
					result.push_back(val.substr(prev, val.size() - prev));
					break;
				} else {
					result.push_back(val.substr(prev, pos - prev));
				}
				prev = pos + splitor.size();
			}
		}

		return result;
	}

	std::string get_time_string(const std::tm& time, const std::string& format) {
		std::stringstream ss;
		ss << std::put_time(&time, format.c_str());
		std::string result = ss.str();
		return result;
	}


	std::string get_local_time_string(const std::time_t time, const std::string& format) {
		std::stringstream ss;
		std::tm t;
		localtime_s(&t, &time);
		ss << std::put_time(&t, format.c_str());
		std::string result = ss.str();
		return result;
	}

	std::string get_local_time_string(const std::string& format) {
		std::time_t time = std::time(NULL);
		std::stringstream ss;
		std::tm t;
		localtime_s(&t, &time);
		ss << std::put_time(&t, format.c_str());
		std::string result = ss.str();
		return result;
	}

	std::string get_gmt_time_string(const std::time_t time, const std::string& format) {
		std::stringstream ss;
		std::tm t;
		gmtime_s(&t, &time);
		ss << std::put_time(&t, format.c_str());
		std::string result = ss.str();
		return result;
	}

	std::string get_gmt_time_string(const std::string& format) {
		std::time_t time = std::time(NULL);
		std::stringstream ss;
		std::tm t;
		gmtime_s(&t, &time);
		ss << std::put_time(&t, format.c_str());
		std::string result = ss.str();
		return result;
	}

	void sleep(unsigned long millseconds) {
		Sleep(millseconds);
	}
}