#pragma once
#define _USE_MATH_DEFINES
#include <algorithm>
#include <array>
#include <vector>
#include <queue>
#include <map>
#include <utility>
#include <limits>
#include <math.h>
#include "BitmapUtility.h"
#include "BitmapFile.h"
#include "commontypes.h"
#include "IN3File.h"

// Forward declaration of class dependencies
class IN3File;

class Codec : public BitmapUtility
{
private:
	// Convert an RGB bitmap to a YUV vector structure
	YUVVectors<INT8> cvtBmpToYUVVector(BitmapFile* bitmapFile);

	// Compress the YUV vectors
	std::pair<IN3Header<INT8>, YUVVectors<bool>> compressYUVVector(
		const YUVVectors<INT8>& yuvVectors);

	// Huffman coding utility types and functions
	struct SymbolWithCount {
		INT32 Symbol;
		UINT32 Count;
		bool operator>(const SymbolWithCount& a) const {
			return Count > a.Count;
		}
	};

	// Frequency table type
	template <typename T>
	using FrequencyTable = std::array<SymbolWithCount, std::numeric_limits<T>::max() - std::numeric_limits<T>::min() + 1>;

	// Frequency count function
	template <typename T>
	FrequencyTable<T> freqCount(const std::vector<T>& symbols);

	// Huffman coding of symbols
	template <typename T>
	std::pair<LengthTable<T>, std::vector<bool>> huffmanEncode(const std::vector<T>& input);

	// Huffman decoding of symbols
	template <typename T>
	std::vector<T> huffmanDecode(
		const LengthTable<T>& lengthTable,
		std::vector<bool>& input,
		size_t numToDecode = std::numeric_limits<size_t>::max());

	// Decompression functions

	// Extract data from IM3 file
	std::pair<IN3Header<INT8>, YUVVectors<bool>> cvtIn3ToYUVVector(IN3File* in3File);

	// Entropy decoding
	YUVVectors<INT8> decompressYUVVector(
		const IN3Header<INT8>& header,
		YUVVectors<bool>& yuvVectors);

	// Convert a YUV vector structure to a RGB bitmap
	BitmapFile* cvtYUVVectorToBmp(const YUVVectors<INT8>& yuvVectors);
public:
	// Compress a bitmap
	IN3File* compress(BitmapFile* bitmapFile);
	// Decompress an IN3
	BitmapFile* decompress(IN3File* in3File);
	Codec();
};

template<typename T>
inline std::pair<LengthTable<T>, std::vector<bool>> Codec::huffmanEncode(const std::vector<T>& input)
{
	// Typedef for Symbol
	typedef INT32 Symbol;
	// Constants
	static const Symbol FIRST_PARENT = std::numeric_limits<T>::max() + 1;
	static const Symbol PARENT_STEP = 1;
	static const Symbol NO_CHILD = std::numeric_limits<T>::min() - 1;
	static const Symbol NO_PARENT = std::numeric_limits<T>::min() - 1;
	// Build the frequency table
	FrequencyTable<T> freqTable = freqCount<T>(input);
	// Sort the frequency table
	std::priority_queue<
		SymbolWithCount,
		std::vector<SymbolWithCount>,
		std::greater<SymbolWithCount>>
		sorted(freqTable.begin(), freqTable.end());
	// Setup the Huffman tree
	// Node
	struct Node {
		Symbol Parent;
		Symbol Left;
		Symbol Right;
	};
	// Map to store the tree
	std::map<Symbol, Node> tree;
	Symbol parentSymbol = FIRST_PARENT;
	while (sorted.size() > 1) {
		// Get and remove the two lowest frequency symbols
		SymbolWithCount a = sorted.top();
		sorted.pop();
		SymbolWithCount b = sorted.top();
		sorted.pop();
		// Create a parent node
		SymbolWithCount parent = { parentSymbol, a.Count + b.Count };
		// Put the parent node back
		sorted.push(parent);
		// Add the parent to the tree
		tree.insert(std::pair<Symbol, Node>(parentSymbol, { NO_PARENT, a.Symbol, b.Symbol }));
		// Add the children to the tree
		if (tree.find(a.Symbol) == tree.end()) {
			tree.insert(std::pair<Symbol, Node>(a.Symbol, { parentSymbol, NO_CHILD, NO_CHILD }));
		}
		else {
			tree[a.Symbol].Parent = parentSymbol;
		}
		if (tree.find(b.Symbol) == tree.end()) {
			tree.insert(std::pair<Symbol, Node>(b.Symbol, { parentSymbol, NO_CHILD, NO_CHILD }));
		}
		else {
			tree[b.Symbol].Parent = parentSymbol;
		}
		parentSymbol += PARENT_STEP;
	}
	// The root node is the last parent
	Symbol root = parentSymbol - PARENT_STEP;
	// Set up the table for storing sorted code lengths
	std::array<
		std::pair<UINT8, T>,
		std::numeric_limits<T>::max() - std::numeric_limits<T>::min() + 1> sortedLengths;
	// Additionally generate a symbol bit length table
	LengthTable<T> lengths;
	// Store the code lengths
	for (Symbol i = std::numeric_limits<T>::min();
		i <= std::numeric_limits<T>::max();
		i++) {
		auto it = tree.find(i);
		Symbol symbol = it->first;
		Node node = tree[symbol];
		UINT8 length = 0;
		while (symbol != root) {
			Symbol nextSymbol = node.Parent;
			Node nextNode = tree[nextSymbol];
			length += 1;
			symbol = nextSymbol;
			node = nextNode;
		}
		INT32 index = i - std::numeric_limits<T>::min();
		lengths[index] = length;
		sortedLengths[index] = { length, static_cast<T>(i) };
	}
	// Sort the code lengths
	std::sort(sortedLengths.begin(), sortedLengths.end());
	// Set up the canonical code table
	std::array<
		std::pair<std::vector<bool>, T>,
		std::numeric_limits<T>::max() - std::numeric_limits<T>::min() + 1> canonicalCodes;
	// Assign canonical codes
	canonicalCodes[0] = { std::vector<bool>(sortedLengths[0].first, false), sortedLengths[0].second };
	for (INT32 i = 1; i < canonicalCodes.size(); i++) {
		// Increment from the previous code for the next code
		std::vector<bool> code = canonicalCodes[i - 1].first;
		UINT8 bitIndex;
		for (bitIndex = static_cast<UINT8>(code.size() - 1); bitIndex >= 0; bitIndex--) {
			if (code[bitIndex]) {
				code[bitIndex] = false;
			} else {
				break;
			}
		}
		code[bitIndex] = true;
		// If the code length increases, append zeroes
		UINT8 length = sortedLengths[i].first;
		while (code.size() != length) {
			code.push_back(false);
		}
		canonicalCodes[i] = { code, sortedLengths[i].second };
	}
	// Build an easier to traverse code table
	std::array<
		std::vector<bool>,
		std::numeric_limits<T>::max() - std::numeric_limits<T>::min() + 1> codes;
	for (Symbol i = std::numeric_limits<T>::min();
		i <= std::numeric_limits<T>::max();
		i++) {
		INT32 index = i - std::numeric_limits<T>::min();
		auto result = std::find_if(
			canonicalCodes.begin(),
			canonicalCodes.end(),
			[i](const std::pair<std::vector<bool>, T>& entry) {
				return entry.second == static_cast<T>(i);
			});
		codes[index] = result->first;
	}
	// Compress the data
	std::vector<bool> compressed;
	for (auto it = input.begin(); it != input.end(); it++) {
		INT32 index = (*it) - std::numeric_limits<T>::min();
		std::vector<bool> code = codes[index];
		compressed.insert(compressed.end(), code.begin(), code.end());
	}
	return std::pair<LengthTable<T>, std::vector<bool>>(lengths, compressed);
}

template<typename T>
inline std::vector<T> Codec::huffmanDecode(
	const LengthTable<T>& lengthTable,
	std::vector<bool>& input,
	size_t numToDecode)
{
	// Typedef for Symbol
	typedef INT32 Symbol;
	// Set up the table for storing sorted code lengths
	std::array<
		std::pair<UINT8, T>,
		std::numeric_limits<T>::max() - std::numeric_limits<T>::min() + 1> sortedLengths;
	// Generate the sorted symbol bit length table
	for (Symbol i = std::numeric_limits<T>::min();
		i <= std::numeric_limits<T>::max();
		i++) {
		INT32 index = i - std::numeric_limits<T>::min();
		sortedLengths[index] = { lengthTable[index], static_cast<T>(i) };
	}
	// Sort the code lengths
	std::sort(sortedLengths.begin(), sortedLengths.end());
	// Set up the canonical code table
	std::array<
		std::pair<std::vector<bool>, T>,
		std::numeric_limits<T>::max() - std::numeric_limits<T>::min() + 1> canonicalCodes;
	// Assign canonical codes
	canonicalCodes[0] = { std::vector<bool>(sortedLengths[0].first, false), sortedLengths[0].second };
	for (INT32 i = 1; i < canonicalCodes.size(); i++) {
		// Increment from the previous code for the next code
		std::vector<bool> code = canonicalCodes[i - 1].first;
		UINT8 bitIndex;
		for (bitIndex = static_cast<UINT8>(code.size() - 1); bitIndex >= 0; bitIndex--) {
			if (code[bitIndex]) {
				code[bitIndex] = false;
			}
			else {
				break;
			}
		}
		code[bitIndex] = true;
		// If the code length increases, append zeroes
		UINT8 length = sortedLengths[i].first;
		while (code.size() != length) {
			code.push_back(false);
		}
		canonicalCodes[i] = { code, sortedLengths[i].second };
	}
	// Decompress the data
	size_t bitsRead = 0;
	std::vector<bool> buffer;
	std::vector<T> decompressed;
	for (auto it = input.begin(); it != input.end(); it++) {
		buffer.push_back(*it);
		bitsRead += 1;
		auto result = std::find_if(
			canonicalCodes.begin(),
			canonicalCodes.end(),
			[buffer](const std::pair<std::vector<bool>, T>& entry) {
				return entry.first == buffer;
			});
		if (result != canonicalCodes.end()) {
			decompressed.push_back(result->second);
			buffer.clear();
			if (decompressed.size() == numToDecode) {
				break;
			}
		}
	}
	// Consume the bits read up to the next byte boundary
	size_t remainder = bitsRead % 8;
	bitsRead = remainder == 0 ? bitsRead : bitsRead + 8 - remainder;
	input.erase(input.begin(), input.begin() + bitsRead);
	return decompressed;
}

template<typename T>
inline Codec::FrequencyTable<T>
Codec::freqCount(const std::vector<T>& symbols)
{
	FrequencyTable<T> table;
	T symbol = std::numeric_limits<T>::min();
	for (auto it = table.begin(); it != table.end(); it++) {
		it->Symbol = symbol;
		it->Count = 0;
		symbol += 1;
	}
	for (auto it = symbols.begin(); it != symbols.end(); it++) {
		table[(*it) - std::numeric_limits<T>::min()].Count += 1;
	}
	return table;
}
