#include "StringUtility.h"
#include <cstdlib>
#include <cwchar>

namespace StringUtility {
	//stringをwstringに変換する
	std::wstring ConvertString(const std::string& str) {
		size_t size = str.size();
		std::wstring wstr(size, L' ');
		mbstowcs_s(nullptr, &wstr[0], size + 1, str.c_str(), size);
		return wstr;
	}
	//wstringをstringに変換する
	std::string ConvertString(const std::wstring& str) {
		size_t size = str.size();
		std::string sstr(size, ' ');
		wcstombs_s(nullptr, &sstr[0], size + 1, str.c_str(), size);
		return sstr;
	}
};
