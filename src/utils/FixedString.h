//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef FIXED_STRING_H
#define FIXED_STRING_H

#pragma once

#include "tier0/platform.h"
#include <string>

class CFixedString
{
public:
	CFixedString()
	{
	}

	CFixedString(const char* pStr) :
		m_String(pStr ? pStr : "")
	{
	}

	CFixedString(const std::string& str) :
		m_String(str)
	{
	}

	const char* Get() const
	{
		return m_String.c_str();
	}

	const char* AsCharPtr() const
	{
		return m_String.c_str();
	}

	const char* c_str() const
	{
		return m_String.c_str();
	}

	CFixedString operator+(const CFixedString& other) const
	{
		return CFixedString(m_String + other.m_String);
	}

	CFixedString operator+(const char* pStr) const
	{
		return CFixedString(m_String + std::string(pStr ? pStr : ""));
	}

	static const size_t npos = (size_t)-1;

	size_t find(const char* pStr, size_t pos = 0) const
	{
		return m_String.find(pStr ? pStr : "", pos);
	}

	uint64 Length() const
	{
		return uint64(m_String.length());
	}

	bool IsEmpty() const
	{
		return m_String.empty();
	}

	void Clear()
	{
		m_String.clear();
	}

	CFixedString& operator=(const char* pStr)
	{
		m_String = pStr ? pStr : "";
		return *this;
	}

	bool operator==(const CFixedString& other) const
	{
		return m_String == other.m_String;
	}

	bool operator!=(const CFixedString& other) const
	{
		return m_String != other.m_String;
	}

	bool operator<(const CFixedString& other) const
	{
		return m_String < other.m_String;
	}
private:
	std::string m_String;
};

inline CFixedString operator+(const char* pLeft, const CFixedString& right)
{
	return CFixedString(std::string(pLeft ? pLeft : "") + right.c_str());
}

#endif // FIXED_STRING_H