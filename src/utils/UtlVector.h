//===----------------------------------------------------------------------===//
//
// Part of the gcmgl, under the MIT License.
// See LICENSE for license information.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef UTL_VECTOR_H
#define UTL_VECTOR_H

#pragma once

#include "tier0/platform.h"
#include "tier0/dbg.h"

template <class T>
class CUtlVector
{
public:
	typedef T ElemType_t;
	typedef int32 IndexType_t;

	CUtlVector(int32 initialCapacity = 4) :
		m_Count(0),
		m_Capacity(initialCapacity > 0 ? initialCapacity : 4)
	{
		m_Elements = new T[m_Capacity];
	}

	CUtlVector(const CUtlVector& other)
	{
		m_Count = other.m_Count;
		m_Capacity = other.m_Capacity;
		m_Elements = new T[m_Capacity];
		for (int32 i = 0; i < m_Count; i++)
		{
			m_Elements[i] = other.m_Elements[i];
		}
	}

	~CUtlVector()
	{
		delete[] m_Elements;
	}

	CUtlVector& operator=(const CUtlVector& other)
	{
		if (this != &other)
		{
			delete[] m_Elements;
			m_Count = other.m_Count;
			m_Capacity = other.m_Capacity;
			m_Elements = new T[m_Capacity];
			for (int32 i = 0; i < m_Count; i++)
			{
				m_Elements[i] = other.m_Elements[i];
			}
		}

		return *this;
	}

	void EnsureCapacity(int32 neededCapacity)
	{
		if (neededCapacity <= m_Capacity)
		{
			return;
		}

		int32 newCapacity = m_Capacity > 0 ? m_Capacity * 2 : 4;
		if (newCapacity < neededCapacity)
			newCapacity = neededCapacity;

		T* newElements = new T[newCapacity];
		for (int32 i = 0; i < m_Count; i++)
		{
			newElements[i] = m_Elements[i];
		}

		delete[] m_Elements;
		m_Elements = newElements;
		m_Capacity = newCapacity;
	}

	int32 AddToTail(const T& value)
	{
		EnsureCapacity(m_Count + 1);
		m_Elements[m_Count] = value;

		return m_Count++;
	}

	int32 InsertBefore(int32 index, const T& value)
	{
		if (UNLIKELY(index < 0 || index > m_Count))
		{
			AssertMsg(false, "CUtlVector: Invalid insertion index");

			return -1;
		}

		EnsureCapacity(m_Count + 1);

		// Shift elements forward
		for (int32 i = m_Count; i > index; --i)
		{
			m_Elements[i] = m_Elements[i - 1];
		}

		m_Elements[index] = value;
		m_Count++;

		return index;
	}

	void RemoveAt(int32 index)
	{
		if (UNLIKELY(index < 0 || index >= m_Count))
		{
			AssertMsg(false, "CUtlVector: Index out of bounds");

			return;
		}

		// Shift elements down
		for (int32 i = index; i < m_Count - 1; i++)
		{
			m_Elements[i] = m_Elements[i + 1];
		}

		m_Count--;
	}

	void FastRemove(int32 index)
	{
		if (UNLIKELY(index < 0 || index >= m_Count))
		{
			AssertMsg(false, "CUtlVector: Index out of bounds");

			return;
		}

		m_Elements[index] = m_Elements[m_Count - 1];
		m_Count--;
	}

	bool FindAndRemove(const T& value)
	{
		for (int32 i = 0; i < m_Count; i++)
		{
			if (m_Elements[i] == value)
			{
				RemoveAt(i);

				return true;
			}
		}

		return false;
	}

	void RemoveAll()
	{
		m_Count = 0;
	}

	void SetCount(int32 count)
	{
		EnsureCapacity(count);
		m_Count = count;
	}

	int32 Count() const
	{
		return m_Count;
	}

	int32 size() const
	{
		return m_Count;
	}

	int32 Capacity() const
	{
		return m_Capacity;
	}

	bool IsEmpty() const
	{
		return m_Count == 0;
	}

	T& operator[](int32 index)
	{
		AssertMsg(
			index >= 0 && index < m_Count,
			"CUtlVector: Index out of bounds");

		return m_Elements[index];
	}

	const T& operator[](int32 index) const
	{
		AssertMsg(
			index >= 0 && index < m_Count,
			"CUtlVector: Index out of bounds");

		return m_Elements[index];
	}

	T& Head()
	{
		AssertMsg(m_Count > 0, "CUtlVector: Accessing Head of empty vector");

		return m_Elements[0];
	}

	const T& Head() const
	{
		AssertMsg(m_Count > 0, "CUtlVector: Accessing Head of empty vector");

		return m_Elements[0];
	}

	T& Tail()
	{
		AssertMsg(m_Count > 0, "CUtlVector: Accessing Tail of empty vector");

		return m_Elements[m_Count - 1];
	}

	const T& Tail() const
	{
		AssertMsg(m_Count > 0, "CUtlVector: Accessing Tail of empty vector");

		return m_Elements[m_Count - 1];
	}

	T* Base()
	{
		return m_Elements;
	}

	const T* Base() const
	{
		return m_Elements;
	}

	int32 Find(const T& value) const
	{
		for (int32 i = 0; i < m_Count; i++)
		{
			if (m_Elements[i] == value)
			{
				return i;
			}
		}

		return -1;
	}

	bool HasElement(const T& value) const
	{
		return Find(value) != -1;
	}
private:
	T* m_Elements;
	int32 m_Count;
	int32 m_Capacity;
};

#endif // UTL_VECTOR_H