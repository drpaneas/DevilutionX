/**
 * @file dc_save_lzo.cpp
 * @brief LZO compression for Dreamcast VMU saves
 *
 * VMU has very limited space (~100KB usable), so we compress save data
 * using LZO to maximize the number of saves that can fit.
 *
 * This implementation is inspired by dca3-game (GTA III Dreamcast port)
 * which uses the same compression scheme for its VMU saves.
 *
 * File format:
 *   Compressed:
 *     [4 bytes] compressed size | 0x80000000
 *     [4 bytes] original uncompressed size
 *     [N bytes] LZO-compressed data
 *   Uncompressed:
 *     [4 bytes] uncompressed size (bit 31 clear)
 *     [N bytes] raw data
 */

#ifdef __DREAMCAST__

#include "dc_save_lzo.hpp"
#include "minilzo.h"
#include "utils/log.hpp"
#include <cstdlib>
#include <cstring>
#include <limits>

namespace devilution {
namespace dc {

static constexpr size_t LZO_WORK_MEM_SIZE = LZO1X_1_MEM_COMPRESS;
static constexpr uint32_t COMPRESSED_FLAG = 0x80000000;
static constexpr size_t MaxSaveDataSize = 512 * 1024; // Corruption guard for VMU save payloads
static bool g_lzoInitialized = false;

bool InitLzo()
{
	if (g_lzoInitialized)
		return true;

	if (lzo_init() != LZO_E_OK) {
		LogError("[DC LZO] Failed to initialize LZO library");
		return false;
	}

	g_lzoInitialized = true;
	return true;
}

std::unique_ptr<std::byte[]> CompressData(const std::byte *data, size_t size, size_t &outSize)
{
	if (!InitLzo()) {
		outSize = 0;
		return nullptr;
	}

	auto workMem = std::make_unique<std::byte[]>(LZO_WORK_MEM_SIZE);

	// LZO guarantees output won't exceed input + input/16 + 64 + 3
	size_t maxCompressedSize = size + (size / 16) + 64 + 3;
	auto compressedBuf = std::make_unique<std::byte[]>(maxCompressedSize);

	lzo_uint compressedLen = 0;
	int result = lzo1x_1_compress(
	    reinterpret_cast<const unsigned char *>(data),
	    static_cast<lzo_uint>(size),
	    reinterpret_cast<unsigned char *>(compressedBuf.get()),
	    &compressedLen,
	    workMem.get());

	if (result != LZO_E_OK) {
		LogError("[DC LZO] Compression failed with error {}", result);
		outSize = 0;
		return nullptr;
	}

	if (compressedLen >= size) {
		outSize = 0;
		return nullptr;
	}

	outSize = compressedLen;
	return compressedBuf;
}

std::unique_ptr<std::byte[]> DecompressData(const std::byte *compressedData, size_t compressedSize,
    size_t expectedSize, size_t &outSize)
{
	if (!InitLzo()) {
		outSize = 0;
		return nullptr;
	}
	if (compressedData == nullptr || compressedSize == 0 || expectedSize == 0 || expectedSize > MaxSaveDataSize) {
		outSize = 0;
		return nullptr;
	}

	auto outputBuf = std::make_unique<std::byte[]>(expectedSize);

	lzo_uint decompressedLen = 0;
	int result = lzo1x_decompress_safe(
	    reinterpret_cast<const unsigned char *>(compressedData),
	    static_cast<lzo_uint>(compressedSize),
	    reinterpret_cast<unsigned char *>(outputBuf.get()),
	    &decompressedLen,
	    nullptr);

	if (result != LZO_E_OK) {
		LogError("[DC LZO] Decompression failed with error {}", result);
		outSize = 0;
		return nullptr;
	}
	if (decompressedLen != expectedSize) {
		LogError("[DC LZO] Decompressed size mismatch. Expected {}, got {}", expectedSize, decompressedLen);
		outSize = 0;
		return nullptr;
	}

	outSize = decompressedLen;
	return outputBuf;
}

bool WriteCompressedFile(const char *path, const std::byte *data, size_t size)
{
	if (!InitLzo()) {
		return false;
	}

	// Validate input BEFORE opening the file - fopen("wb") truncates,
	// which would destroy an existing save if we then bail out.
	if (data == nullptr || size == 0 || size > MaxSaveDataSize || size > std::numeric_limits<uint32_t>::max()) {
		LogError("[DC LZO] Refusing to write invalid save payload ({} bytes) to {}", size, path);
		return false;
	}

	FILE *file = std::fopen(path, "wb");
	if (file == nullptr) {
		LogError("[DC LZO] Failed to open {} for writing", path);
		return false;
	}

	size_t compressedSize = 0;
	auto compressedData = CompressData(data, size, compressedSize);

	if (compressedData && compressedSize < size) {
		// Write compressed: header (flagged size) + original size + compressed data
		uint32_t header = static_cast<uint32_t>(compressedSize) | COMPRESSED_FLAG;
		uint32_t originalSize = static_cast<uint32_t>(size);
		if (std::fwrite(&header, sizeof(header), 1, file) != 1
		    || std::fwrite(&originalSize, sizeof(originalSize), 1, file) != 1
		    || std::fwrite(compressedData.get(), compressedSize, 1, file) != 1) {
			LogError("[DC LZO] Failed to write compressed data to {}", path);
			std::fclose(file);
			return false;
		}
		LogVerbose("[DC LZO] Compressed {} -> {} bytes ({:.1f}%)",
		    size, compressedSize, 100.0f * compressedSize / size);
	} else {
		// Write uncompressed: header (raw size) + raw data
		uint32_t header = static_cast<uint32_t>(size);
		if (std::fwrite(&header, sizeof(header), 1, file) != 1
		    || std::fwrite(data, size, 1, file) != 1) {
			LogError("[DC LZO] Failed to write uncompressed data to {}", path);
			std::fclose(file);
			return false;
		}
		LogVerbose("[DC LZO] Storing uncompressed {} bytes", size);
	}

	std::fclose(file);
	return true;
}

std::unique_ptr<std::byte[]> ReadCompressedFile(const char *path, size_t &outSize)
{
	if (!InitLzo()) {
		outSize = 0;
		return nullptr;
	}

	FILE *file = std::fopen(path, "rb");
	if (file == nullptr) {
		outSize = 0;
		return nullptr;
	}

	uint32_t header;
	if (std::fread(&header, sizeof(header), 1, file) != 1) {
		LogError("[DC LZO] Failed to read header from {}", path);
		std::fclose(file);
		outSize = 0;
		return nullptr;
	}

	bool isCompressed = (header & COMPRESSED_FLAG) != 0;
	size_t dataSize = header & ~COMPRESSED_FLAG;
	if (dataSize == 0 || dataSize > MaxSaveDataSize) {
		LogError("[DC LZO] Invalid save payload size {} in {}", dataSize, path);
		std::fclose(file);
		outSize = 0;
		return nullptr;
	}

	if (!isCompressed) {
		// Uncompressed: header was the size, data follows directly
		auto fileData = std::make_unique<std::byte[]>(dataSize);
		if (std::fread(fileData.get(), dataSize, 1, file) != 1) {
			LogError("[DC LZO] Failed to read data from {}", path);
			std::fclose(file);
			outSize = 0;
			return nullptr;
		}
		std::fclose(file);
		outSize = dataSize;
		return fileData;
	}

	// Compressed: read original size, then compressed data
	uint32_t originalSize;
	if (std::fread(&originalSize, sizeof(originalSize), 1, file) != 1) {
		LogError("[DC LZO] Failed to read original size from {}", path);
		std::fclose(file);
		outSize = 0;
		return nullptr;
	}
	if (originalSize == 0 || originalSize > MaxSaveDataSize) {
		LogError("[DC LZO] Invalid uncompressed size {} in {}", originalSize, path);
		std::fclose(file);
		outSize = 0;
		return nullptr;
	}

	auto compressedData = std::make_unique<std::byte[]>(dataSize);
	if (std::fread(compressedData.get(), dataSize, 1, file) != 1) {
		LogError("[DC LZO] Failed to read compressed data from {}", path);
		std::fclose(file);
		outSize = 0;
		return nullptr;
	}
	std::fclose(file);

	// Decompress directly into exact-sized buffer
	auto outputBuf = std::make_unique<std::byte[]>(originalSize);
	lzo_uint decompressedLen = 0;
	int result = lzo1x_decompress_safe(
	    reinterpret_cast<const unsigned char *>(compressedData.get()),
	    static_cast<lzo_uint>(dataSize),
	    reinterpret_cast<unsigned char *>(outputBuf.get()),
	    &decompressedLen,
	    nullptr);

	if (result != LZO_E_OK) {
		LogError("[DC LZO] Decompression failed for {}: error {}", path, result);
		outSize = 0;
		return nullptr;
	}
	if (decompressedLen != originalSize) {
		LogError("[DC LZO] Decompressed size mismatch for {}: expected {}, got {}", path, originalSize, decompressedLen);
		outSize = 0;
		return nullptr;
	}

	outSize = decompressedLen;
	LogVerbose("[DC LZO] Decompressed {} -> {} bytes", dataSize, decompressedLen);
	return outputBuf;
}

} // namespace dc
} // namespace devilution

#endif // __DREAMCAST__
