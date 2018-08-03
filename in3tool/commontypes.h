#pragma once
#include "stdafx.h"

#include <array>
#include <vector>
#include <limits>

// Canonical Huffman coding length table
template <typename T>
using LengthTable = std::array<UINT8, std::numeric_limits<T>::max() - std::numeric_limits<T>::min() + 1>;

// Structures
// File structure types
// Structure packing set to 1-byte to have continuous reading
#pragma pack(push, 1)
// IN3 File Header With Tables
template <typename T>
struct IN3Header {
	UINT8 MagicByteI = 73; // 'I' == 73
	UINT8 MagicByteN = 78; // 'N' == 78
	UINT16 Width;
	UINT16 Height;
	UINT32 YSize;
	UINT32 USize;
	UINT32 VSize;
	LengthTable<T> YTable;
	LengthTable<T> UTable;
	LengthTable<T> VTable;
};
#pragma pack(pop)
// Y, U, and V vectors
template <typename T>
struct YUVVectors {
	std::vector<T> Y;
	std::vector<T> U;
	std::vector<T> V;
	UINT64 Width;
	UINT64 Height;
	UINT64 getWidth() const;
	UINT64 getHeight() const;
	YUVVectors();
	YUVVectors(
		const UINT64 width,
		const UINT64 height);
};

template<typename T>
inline UINT64 YUVVectors<T>::getWidth() const
{
	return Width;
}

template<typename T>
inline UINT64 YUVVectors<T>::getHeight() const
{
	return Height;
}

template<typename T>
inline YUVVectors<T>::YUVVectors()
{
}

template<typename T>
inline YUVVectors<T>::YUVVectors(
	const UINT64 width,
	const UINT64 height)
	: Width(width),
	  Height(height)
{
	const UINT64 size = width * height;
	Y.resize(size);
	U.resize(size);
	V.resize(size);
}
