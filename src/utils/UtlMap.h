//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef UTL_MAP_H
#define UTL_MAP_H

#pragma once

#include "tier0/platform.h"
#include "tier0/dbg.h"

template <class K, class V, int MAX_ELEMENTS = 256>
class CUtlMap
{
public:
	typedef int32 Index_t;

	struct Node_t
	{
		K m_Key;
		V m_Value;
	};

	CUtlMap() :
		m_Count(0)
	{
	}

	CUtlMap(const CUtlMap& other)
	{
		m_Count = other.m_Count;
		for (int32 i = 0; i < m_Count; i++)
		{
			m_Elements[i] = other.m_Elements[i];
		}
	}

	~CUtlMap()
	{
		RemoveAll();
	}

	CUtlMap& operator=(const CUtlMap& other)
	{
		if (this != &other)
		{
			m_Count = other.m_Count;
			for (int32 i = 0; i < m_Count; i++)
			{
				m_Elements[i] = other.m_Elements[i];
			}
		}

		return *this;
	}

	V& operator[](const K& key)
	{
		int32 index = Find(key);
		if (index != -1)
		{
			return m_Elements[index].m_Value;
		}

		AssertMsg(m_Count < MAX_ELEMENTS, "CUtlMap: Exceeded maximum capacity");
		
		int32 insertPos = FindInsertionPos(key);
		
		// Shift elements forward
		for (int32 i = m_Count; i > insertPos; --i)
		{
			m_Elements[i] = m_Elements[i - 1];
		}

		m_Elements[insertPos].m_Key = key;
		m_Count++;

		return m_Elements[insertPos].m_Value;
	}

	int32 Insert(const K& key, const V& value)
	{
		int32 index = Find(key);
		if (index != -1)
		{
			m_Elements[index].m_Value = value;

			return index;
		}

		if (UNLIKELY(m_Count >= MAX_ELEMENTS))
		{
			AssertMsg(false, "CUtlMap: Exceeded maximum capacity");

			return -1;
		}

		int32 insertPos = FindInsertionPos(key);

		// Shift elements forward
		for (int32 i = m_Count; i > insertPos; --i)
		{
			m_Elements[i] = m_Elements[i - 1];
		}

		m_Elements[insertPos].m_Key = key;
		m_Elements[insertPos].m_Value = value;
		m_Count++;

		return insertPos;
	}

	int32 Find(const K& key) const
	{
		// Binary search
		int32 low = 0;
		int32 high = m_Count - 1;

		while (low <= high)
		{
			int32 mid = low + (high - low) / 2;
			
			// Needs operator== and operator< on type K
			if (m_Elements[mid].m_Key == key)
			{
				return mid;
			}
			
			// Store in ascending order
			if (m_Elements[mid].m_Key < key)
			{
				low = mid + 1;
			}
			else
			{
				high = mid - 1;
			}
		}

		return -1;
	}

	void RemoveAt(int32 index)
	{
		if (UNLIKELY(index < 0 || index >= m_Count))
		{
			AssertMsg(false, "CUtlMap: Index out of bounds");

			return;
		}

		// Shift elements down
		for (int32 i = index; i < m_Count - 1; i++)
		{
			m_Elements[i] = m_Elements[i + 1];
		}

		m_Count--;
	}

	bool FindAndRemove(const K& key)
	{
		int32 index = Find(key);
		if (index != -1)
		{
			RemoveAt(index);

			return true;
		}

		return false;
	}

	void RemoveAll()
	{
		m_Count = 0;
	}

	bool Remove(const K& key)
	{
		return FindAndRemove(key);
	}

	Index_t FirstInorder() const
	{
		return m_Count > 0 ? 0 : InvalidIndex();
	}

	Index_t NextInorder(Index_t i) const
	{
		if (i >= 0 && i < m_Count - 1)
		{
			return i + 1;
		}

		return InvalidIndex();
	}

	int32 Count() const
	{
		return m_Count;
	}

	int32 Capacity() const
	{
		return MAX_ELEMENTS;
	}

	bool IsEmpty() const
	{
		return m_Count == 0;
	}

	bool IsFull() const
	{
		return m_Count >= MAX_ELEMENTS;
	}
	
	const K& Key(int32 index) const
	{
		AssertMsg(
			index >= 0 && index < m_Count,
			"CUtlMap: Index out of bounds");

		return m_Elements[index].m_Key;
	}

	V& Element(int32 index)
	{
		AssertMsg(
			index >= 0 && index < m_Count,
			"CUtlMap: Index out of bounds");

		return m_Elements[index].m_Value;
	}

	const V& Element(int32 index) const
	{
		AssertMsg(
			index >= 0 && index < m_Count,
			"CUtlMap: Index out of bounds");

		return m_Elements[index].m_Value;
	}

	static const int32 InvalidIndex()
	{
		return -1;
	}

	bool IsValidIndex(int32 index) const
	{
		return index >= 0 && index < m_Count;
	}
private:
	int32 FindInsertionPos(const K& key) const
	{
		// Binary search for insertion point
		int32 low = 0;
		int32 high = m_Count - 1;

		while (low <= high)
		{
			int32 mid = low + (high - low) / 2;
			if (m_Elements[mid].m_Key < key)
			{
				low = mid + 1;
			}
			else
			{
				high = mid - 1;
			}
		}

		return low;
	}

	Node_t m_Elements[MAX_ELEMENTS];
	int32 m_Count;
};

#endif // UTL_MAP_H
