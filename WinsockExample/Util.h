#pragma once

#include "pch.h"
#include <string>
#include <locale>
#include <codecvt>

/**
 *
 */
class Util
{
public:
	Util();
	~Util();

	static bool InTheRange(double Subject, double RangeA, double RangeB);

	static std::string wstos(const wchar_t* Data);
	static std::wstring stows(const char* Data);

};
