#include "stdafx.h"
#include <cstring>
#include "commontypes.h"
#include "IN3File.h"

void IN3File::Save(HANDLE fileHandle)
{
	static const DWORD headerSize = sizeof(Header);
	DWORD bytesWritten;
	WriteFile(
		fileHandle,
		&Header,
		headerSize,
		&bytesWritten,
		NULL);
	for (UINT8 i = 0; i < 3; i++) {
		std::vector<bool>* data = NULL;
		switch (i) {
		case 0:
			data = &Vectors.Y;
			break;
		case 1:
			data = &Vectors.U;
			break;
		case 2:
			data = &Vectors.V;
			break;
		}
		size_t numBits = data->size();
		DWORD bufSize = static_cast<DWORD>(ceil(numBits / 8.0));
		BYTE* outputBuffer = new BYTE[bufSize];
		BYTE b;
		for (size_t bit = 0; bit < numBits; bit++) {
			BYTE x = !!(data->at(bit));
			b ^= (-x ^ b) & (1 << (bit % 8));
			if ((bit + 1) % 8 == 0) {
				outputBuffer[bit / 8] = b;
			}
		}
		outputBuffer[bufSize - 1] = b;
		WriteFile(
			fileHandle,
			outputBuffer,
			bufSize,
			&bytesWritten,
			NULL);
		delete[] outputBuffer;
	}
	CloseHandle(fileHandle);
}

IN3Header<INT8> IN3File::getHeader()
{
	return Header;
}

std::vector<bool> IN3File::getBitsReadFromFile()
{
	return bitsReadFromFile;
}

IN3File::IN3File(HANDLE fileHandle)
{
	static const UINT64 headerSize = sizeof(Header);
	LARGE_INTEGER fileSizeStruct;
	GetFileSizeEx(fileHandle, &fileSizeStruct);
	UINT64 fileSize = fileSizeStruct.QuadPart;
	BYTE* readBytes = new BYTE[fileSize];
	DWORD bytesRead;
	ReadFile(
		fileHandle,
		readBytes,
		static_cast<DWORD>(fileSize),
		&bytesRead,
		NULL);
	CloseHandle(fileHandle);
	std::memcpy(
		&Header,
		readBytes,
		headerSize);
	for (UINT64 i = headerSize; i < fileSize; i++) {
		BYTE readByte = readBytes[i];
		BYTE mask = 0x01;
		for (UINT8 bitIndex = 0; bitIndex < 8; bitIndex++) {
			bitsReadFromFile.push_back(!!(readByte & mask));
			mask <<= 1;
		}
	}
	delete[] readBytes;
}

IN3File::IN3File(
	IN3Header<INT8> header,
	YUVVectors<bool> vectors)
	: Header(header),
	  Vectors(vectors)
{
}

IN3File::~IN3File()
{
}
