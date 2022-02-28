// Fill out your copyright notice in the Description page of Project Settings.

#include "pch.h"
#include "Util.h"

Util::Util()
{
}

Util::~Util()
{
}
bool Util::InTheRange(double Subject, double RangeA, double RangeB)
{
	double max, min;

	max = RangeA >= RangeB ? RangeA : RangeB;
	min = RangeA < RangeB ? RangeA : RangeB;

	return (Subject <= max && Subject >= min);
}

std::string Util::wstos(const wchar_t * Data)
{
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	std::string rtn = converter.to_bytes(Data);

	return rtn;
}

std::wstring Util::stows(const char * Data)
{
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	std::wstring rtn = converter.from_bytes(Data);

	return rtn;
}

