#pragma once
#include <string>
#include <vector>
#include <ctime>

namespace util {

	std::string trim(const std::string& val);

	std::string trim_left(const std::string& val);

	std::string trim_right(const std::string& val);

	std::vector<std::string> split(const std::string& val, const std::string& splitor);

	std::string get_time_string(const std::tm& time, const std::string& format);

	std::string get_local_time_string(const std::time_t time, const std::string& format);

	std::string get_local_time_string(const std::string& format);

	std::string get_gmt_time_string(const std::time_t time, const std::string& format);

	std::string get_gmt_time_string(const std::string& format);

	void sleep(unsigned long millseconds);

}