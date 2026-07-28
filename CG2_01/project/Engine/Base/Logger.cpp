#include "Logger.h"
#include <iostream>

namespace Logger {
	void Log(const std::string& message) {
		std::cout << message;
	}
}
