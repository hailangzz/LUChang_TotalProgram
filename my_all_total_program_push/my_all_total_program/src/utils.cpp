#include "utils.h"
//#include "constants.h"


std::string to_string(const std::wstring& wstr)
{
	unsigned len = wstr.size() * 4;
	setlocale(LC_CTYPE, "");
	char* p = new char[len];
	wcstombs(p, wstr.c_str(), len);
	std::string str(p);
	delete[] p;
	return str;
}

std::wstring to_wstring(const std::string& str)
{
	unsigned len = str.size() * 2;
	setlocale(LC_CTYPE, "");
	wchar_t* p = new wchar_t[len];
	mbstowcs(p, str.c_str(), len);
	std::wstring wstr(p);
	delete[] p;
	return wstr;
}
