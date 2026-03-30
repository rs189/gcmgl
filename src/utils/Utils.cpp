//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#include "Utils.h"
#include <stdio.h>

// Reads a text file into a string
CFixedString CUtils::ReadFile(const CFixedString& path)
{
	CFixedString fullPath = path;
#ifdef PLATFORM_PS3
	if (fullPath.find("/dev_hdd0/") != 0)
	{
		fullPath = "/dev_hdd0/game/GCGL00001/USRDIR/" + fullPath;
	}
#endif

	CUtlVector<uint8> bytes = ReadBinaryFile(fullPath);
	if (bytes.Count() > 0)
	{
		bytes.AddToTail('\0');
	}

	CFixedString result;
	if (bytes.Count() > 0)
	{
		result = reinterpret_cast<const char*>(bytes.Base());
	}

	return result;
}

// Reads a binary file into a vector of bytes
CUtlVector<uint8> CUtils::ReadBinaryFile(const CFixedString& path)
{
	CFixedString fullPath = path;
#ifdef PLATFORM_PS3
	if (fullPath.find("/dev_hdd0/") != 0)
	{
		fullPath = "/dev_hdd0/game/GCGL00001/USRDIR/" + fullPath;
	}
#endif

	FILE* pFile = fopen(fullPath.c_str(), "rb");
	if (!pFile)
	{
		return CUtlVector<uint8>();
	}

	fseek(pFile, 0, SEEK_END);
	long size = ftell(pFile);
	fseek(pFile, 0, SEEK_SET);

	if (size <= 0)
	{
		fclose(pFile);

		return CUtlVector<uint8>();
	}

	CUtlVector<uint8> buffer;
	buffer.SetCount(int32(size));

	fread(buffer.Base(), 1, size_t(size), pFile);
	fclose(pFile);
	
	return buffer;
}