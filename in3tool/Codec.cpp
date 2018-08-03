#include "stdafx.h"
#include <cmath>
#include <limits>
#include "BitmapUtility.h"
#include "commontypes.h"
#include "Codec.h"
#include "IN3File.h"

YUVVectors<INT8> Codec::cvtBmpToYUVVector(BitmapFile * bitmapFile)
{
	INT32 width = bitmapFile->getWidth();
	INT32 height = bitmapFile->getHeight();
	YUVVectors<INT8> yuv(width, height);
	INT32 i = 0;
	for (INT32 j = 0; j < height; j++) {
		for (INT32 k = 0; k < width; k++) {
			BitmapFile::Pixel pixel = bitmapFile->getPixel(k, j);
			NormalizedRGB rgb = PixelToNormalizedRGB(pixel);
			YUV yuvValue = NormalizedRGBtoYUV(rgb);
			yuv.Y[i] = static_cast<INT8>((yuvValue.Y * 255) - 128);
			yuv.U[i] = static_cast<INT8>((yuvValue.U * 255) - 128);
			yuv.V[i] = static_cast<INT8>((yuvValue.V * 255) - 128);
			i += 1;
		}
	}
	return yuv;
}

std::pair<IN3Header<INT8>, YUVVectors<bool>> Codec::compressYUVVector(
	const YUVVectors<INT8>& yuvVectors)
{
	IN3Header<INT8> header;
	header.Width = static_cast<UINT16>(yuvVectors.getWidth());
	header.Height = static_cast<UINT16>(yuvVectors.getHeight());
	std::pair<LengthTable<INT8>, std::vector<bool>> compressedY =
		huffmanEncode(yuvVectors.Y);
	std::pair<LengthTable<INT8>, std::vector<bool>> compressedU =
		huffmanEncode(yuvVectors.U);
	std::pair<LengthTable<INT8>, std::vector<bool>> compressedV =
		huffmanEncode(yuvVectors.V);
	header.YTable = compressedY.first;
	header.UTable = compressedU.first;
	header.VTable = compressedV.first;
	YUVVectors<bool> compressed;
	compressed.Y = compressedY.second;
	compressed.U = compressedU.second;
	compressed.V = compressedV.second;
	UINT32 ySize = static_cast<UINT32>(compressed.Y.size());
	UINT32 uSize = static_cast<UINT32>(compressed.U.size());
	UINT32 vSize = static_cast<UINT32>(compressed.V.size());
	ySize = (ySize % 8 == 0 ? ySize : ySize + 8 - (ySize % 8)) / 8;
	uSize = (uSize % 8 == 0 ? uSize : uSize + 8 - (uSize % 8)) / 8;
	vSize = (vSize % 8 == 0 ? vSize : vSize + 8 - (vSize % 8)) / 8;
	header.YSize = ySize;
	header.USize = uSize;
	header.VSize = vSize;
	return std::pair<IN3Header<INT8>, YUVVectors<bool>>(header, compressed);
}

std::pair<IN3Header<INT8>, YUVVectors<bool>> Codec::cvtIn3ToYUVVector(IN3File * in3File)
{
	IN3Header<INT8> header = in3File->getHeader();
	std::vector<bool> bitsReadFromFile = in3File->getBitsReadFromFile();
	YUVVectors<bool> compressed;
	compressed.Y.resize(header.YSize * 8);
	compressed.U.resize(header.USize * 8);
	compressed.V.resize(header.VSize * 8);
	std::copy(
		bitsReadFromFile.begin(),
		bitsReadFromFile.begin() + header.YSize * 8,
		compressed.Y.begin());
	bitsReadFromFile.erase(
		bitsReadFromFile.begin(),
		bitsReadFromFile.begin() + header.YSize * 8);
	std::copy(
		bitsReadFromFile.begin(),
		bitsReadFromFile.begin() + header.USize * 8,
		compressed.U.begin());
	bitsReadFromFile.erase(
		bitsReadFromFile.begin(),
		bitsReadFromFile.begin() + header.USize * 8);
	std::copy(
		bitsReadFromFile.begin(),
		bitsReadFromFile.begin() + header.VSize * 8,
		compressed.V.begin());
	bitsReadFromFile.erase(
		bitsReadFromFile.begin(),
		bitsReadFromFile.begin() + header.VSize * 8);
	return std::pair<IN3Header<INT8>, YUVVectors<bool>>(header, compressed);
}

YUVVectors<INT8> Codec::decompressYUVVector(
	const IN3Header<INT8>& header,
	YUVVectors<bool>& yuvVectors)
{
	INT32 numSymbols = header.Width * header.Height;
	std::vector<INT8> yVec = huffmanDecode<INT8>(
		header.YTable,
		yuvVectors.Y,
		numSymbols);
	std::vector<INT8> uVec = huffmanDecode<INT8>(
		header.UTable,
		yuvVectors.U,
		numSymbols);
	std::vector<INT8> vVec = huffmanDecode<INT8>(
		header.VTable,
		yuvVectors.V,
		numSymbols);
	YUVVectors<INT8> yuvVec(header.Width, header.Height);
	yuvVec.Y = yVec;
	yuvVec.U = uVec;
	yuvVec.V = vVec;
	return yuvVec;
}

BitmapFile * Codec::cvtYUVVectorToBmp(const YUVVectors<INT8>& yuvVectors)
{
	INT32 width = static_cast<INT32>(yuvVectors.getWidth());
	INT32 height = static_cast<INT32>(yuvVectors.getHeight());
	BitmapFile* bitmapFile = new BitmapFile(width, height);
	INT32 i = 0;
	for (INT32 j = 0; j < height; j++) {
		for (INT32 k = 0; k < width; k++) {
			YUV yuvValue;
			yuvValue.Y = static_cast<DOUBLE>(yuvVectors.Y[i] + 128) / 255.0;
			yuvValue.U = static_cast<DOUBLE>(yuvVectors.U[i] + 128) / 255.0;
			yuvValue.V = static_cast<DOUBLE>(yuvVectors.V[i] + 128) / 255.0;
			NormalizedRGB rgb = YUVtoNormalizedRGB(yuvValue);
			BitmapFile::Pixel pixel = NormalizedRGBtoPixel(rgb);
			bitmapFile->setPixel(k, j, pixel);
			i++;
		}
	}
	return bitmapFile;
}

IN3File* Codec::compress(BitmapFile * bitmapFile)
{
	YUVVectors<INT8> yuv = cvtBmpToYUVVector(bitmapFile);
	std::pair<IN3Header<INT8>, YUVVectors<bool>> compressed =
		compressYUVVector(yuv);
	IN3File* in3File = new IN3File(
		compressed.first,
		compressed.second);
	return in3File;
}

BitmapFile * Codec::decompress(IN3File* in3File)
{
	std::pair<IN3Header<INT8>, YUVVectors<bool>> compressed =
		cvtIn3ToYUVVector(in3File);
	YUVVectors<INT8> yuv = decompressYUVVector(
		compressed.first,
		compressed.second);
	BitmapFile* bitmapFile = cvtYUVVectorToBmp(yuv);
	return bitmapFile;
}

Codec::Codec()
{
}
