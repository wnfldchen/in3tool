#pragma once
#include <vector>
#include "commontypes.h"
#include "Codec.h"

// Forward declaration of class dependencies
class Codec;

class IN3File
{
private:
	IN3Header<INT8> Header;
	YUVVectors<bool> Vectors;
	std::vector<bool> bitsReadFromFile;
public:
	void Save(HANDLE fileHandle);
	IN3Header<INT8> getHeader();
	std::vector<bool> getBitsReadFromFile();
	IN3File(HANDLE fileHandle);
	IN3File(
		IN3Header<INT8> header,
		YUVVectors<bool> vectors);
	~IN3File();
};

