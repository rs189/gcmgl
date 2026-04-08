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

template <class K, class V, int32 MAX_ELEMENTS = 256>
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
		m_pElements(new Node_t[MAX_ELEMENTS]),
		m_Count(0)
	{
		if (!m_pElements)
		{
			Error(
				"CUtlMap: Failed to allocate memory for %d elements\n",
				MAX_ELEMENTS);
		}
	}

	CUtlMap(const CUtlMap& other) :
		m_pElements(new Node_t[MAX_ELEMENTS]),
		m_Count(other.m_Count)
	{
		if (LIKELY(m_pElements))
		{
			for (int32 i = 0; i < m_Count; i++)
			{
				m_pElements[i] = other.m_pElements[i];
			}
		}
		else
		{
			Error(
				"CUtlMap: Failed to allocate memory for %d elements during copy\n",
				MAX_ELEMENTS);

			m_Count = 0;
		}
	}

	~CUtlMap()
	{
		RemoveAll();

		if (m_pElements)
		{
			delete[] m_pElements;
			m_pElements = GCMGL_NULL;
		}
	}

	CUtlMap& operator=(const CUtlMap& other)
	{
		if (this != &other)
		{
			if (UNLIKELY(!m_pElements))
			{
				m_pElements = new Node_t[MAX_ELEMENTS];
			}

			if (LIKELY(m_pElements))
			{
				m_Count = other.m_Count;
				for (int32 i = 0; i < m_Count; i++)
				{
					m_pElements[i] = other.m_pElements[i];
				}
			}
		}

		return *this;
	}

	V& operator[](const K& key)
	{
		if (UNLIKELY(!m_pElements))
		{
			Error("CUtlMap: Accessing NULL elements!\n");

			static V s_Dummy;
			return s_Dummy;
		}

		int32 index = Find(key);
		if (index != -1)
		{
			return m_pElements[index].m_Value;
		}

		AssertMsg(m_Count < MAX_ELEMENTS, "CUtlMap: Exceeded maximum capacity");
		
		int32 insertPos = FindInsertionPos(key);
		
		// Shift elements forward
		for (int32 i = m_Count; i > insertPos; --i)
		{
			m_pElements[i] = m_pElements[i - 1];
		}

		m_pElements[insertPos].m_Key = key;
		m_Count++;

		return m_pElements[insertPos].m_Value;
	}

	int32 Insert(const K& key, const V& value)
	{
		if (UNLIKELY(!m_pElements))
		{
			Error("CUtlMap: Inserting into NULL map!\n");

			return -1;
		}

		int32 index = Find(key);
		if (index != -1)
		{
			m_pElements[index].m_Value = value;

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
			m_pElements[i] = m_pElements[i - 1];
		}

		m_pElements[insertPos].m_Key = key;
		m_pElements[insertPos].m_Value = value;
		m_Count++;

		return insertPos;
	}

	int32 Find(const K& key) const
	{
		if (UNLIKELY(!m_pElements))
		{
			return -1;
		}

		// Binary search
		int32 low = 0;
		int32 high = m_Count - 1;

		while (low <= high)
		{
			int32 mid = low + (high - low) / 2;
			
			// Needs operator== and operator< on type K
			if (m_pElements[mid].m_Key == key)
			{
				return mid;
			}
			
			// Store in ascending order
			if (m_pElements[mid].m_Key < key)
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
		if (UNLIKELY(!m_pElements || index < 0 || index >= m_Count))
		{
			AssertMsg(false, "CUtlMap: Index out of bounds or NULL map");

			return;
		}

		// Shift elements down
		for (int32 i = index; i < m_Count - 1; i++)
		{
			m_pElements[i] = m_pElements[i + 1];
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
		return (m_pElements && m_Count > 0) ? 0 : InvalidIndex();
	}

	Index_t NextInorder(Index_t i) const
	{
		if (m_pElements && i >= 0 && i < m_Count - 1)
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
			m_pElements && index >= 0 && index < m_Count,
			"CUtlMap: Index out of bounds or NULL map");

		return m_pElements[index].m_Key;
	}

	V& Element(int32 index)
	{
		AssertMsg(
			m_pElements && index >= 0 && index < m_Count,
			"CUtlMap: Index out of bounds or NULL map");

		return m_pElements[index].m_Value;
	}

	const V& Element(int32 index) const
	{
		AssertMsg(
			m_pElements && index >= 0 && index < m_Count,
			"CUtlMap: Index out of bounds or NULL map");

		return m_pElements[index].m_Value;
	}

	static int32 InvalidIndex()
	{
		return -1;
	}

	bool IsValidIndex(int32 index) const
	{
		return m_pElements && index >= 0 && index < m_Count;
	}
private:
	int32 FindInsertionPos(const K& key) const
	{
		if (UNLIKELY(!m_pElements))
		{
			return 0;
		}

		// Binary search for insertion point
		int32 low = 0;
		int32 high = m_Count - 1;

		while (low <= high)
		{
			int32 mid = low + (high - low) / 2;
			if (m_pElements[mid].m_Key < key)
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

	Node_t* m_pElements;
	int32 m_Count;
};

#endif // UTL_MAP_H