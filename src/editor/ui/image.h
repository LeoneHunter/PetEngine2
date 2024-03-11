#pragma once
#include <vector>
#include "runtime/core/types.h"

/**
 * Simple image class
 * Stores raw uncompressed image data
 */
class Image {
public:
	enum class Format {
		NONE,
		RED,
		RGB,
		RGBA,
		RGB16,
		RGBA16,
		SRGB,
		SRGBA,
		LUMINANCE,
	};

	Image() = default;

	Image(size_t inWidth, size_t inHeight, Format inFormat, unsigned char* inData, size_t inDataSize = 0)
		: Width(inWidth)
		, Height(inHeight)
		, m_Format(inFormat)
		, Data() {
		size_t dataSizeBytes = inDataSize;
		if(dataSizeBytes == 0) {
			dataSizeBytes = inWidth * inHeight * GetFormatBytesPerPixel(inFormat);
		}
		Data.resize(dataSizeBytes);
		memcpy(Data.data(), inData, dataSizeBytes);
	}

	unsigned char* GetData() { return Data.data(); }
	const unsigned char* GetData() const { return Data.data(); }

	size_t GetFormatBytesPerPixel(Format inFormat) const {
		switch(inFormat) {
			case Image::Format::RED: return 1;
			case Image::Format::RGB: return 3;
			case Image::Format::RGBA: return 4;
			case Image::Format::RGB16: return 6;
			case Image::Format::RGBA16: return 8;
			case Image::Format::SRGB: return 3;
			case Image::Format::SRGBA: return 4;
			case Image::Format::LUMINANCE: return 1;
		}
		return 0;
	}

public:
	size_t Width = 0;
	size_t Height = 0;
	Format m_Format = Format::NONE;
	std::vector<unsigned char> Data;
};