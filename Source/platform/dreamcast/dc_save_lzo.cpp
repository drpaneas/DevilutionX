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
#include "dc_init.hpp"
#include "minilzo.h"
#include "utils/log.hpp"
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>

#include <dc/fs_vmu.h>
#include <dc/vmu_pkg.h>
#include <kos/fs.h>

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

// Blank 32x32 icon (4bpp = 512 bytes) for VMU file display.
static uint8_t g_blankIcon[512] = { 0 };

bool WriteToVmu(const char *vmuPath, const char *filename,
    const std::byte *data, size_t size)
{
	if (!InitLzo())
		return false;
	if (data == nullptr || size == 0 || size > MaxSaveDataSize)
		return false;

	// Build payload in the same self-describing format as WriteCompressedFile:
	//   Compressed:   [4B compressedSize | COMPRESSED_FLAG][4B origSize][LZO data]
	//   Uncompressed: [4B dataSize][raw data]
	size_t compressedSize = 0;
	auto compressed = CompressData(data, size, compressedSize);

	size_t payloadSize;
	std::unique_ptr<std::byte[]> payload;

	if (compressed && compressedSize < size) {
		payloadSize = sizeof(uint32_t) + sizeof(uint32_t) + compressedSize;
		payload = std::make_unique<std::byte[]>(payloadSize);
		uint32_t hdr = static_cast<uint32_t>(compressedSize) | COMPRESSED_FLAG;
		uint32_t origSize = static_cast<uint32_t>(size);
		std::memcpy(payload.get(), &hdr, sizeof(hdr));
		std::memcpy(payload.get() + sizeof(hdr), &origSize, sizeof(origSize));
		std::memcpy(payload.get() + sizeof(hdr) + sizeof(origSize), compressed.get(), compressedSize);
	} else {
		payloadSize = sizeof(uint32_t) + size;
		payload = std::make_unique<std::byte[]>(payloadSize);
		uint32_t hdr = static_cast<uint32_t>(size);
		std::memcpy(payload.get(), &hdr, sizeof(hdr));
		std::memcpy(payload.get() + sizeof(hdr), data, size);
	}

	// Build VMU metadata for BIOS file display.
	vmu_pkg_t pkg;
	std::memset(&pkg, 0, sizeof(pkg));
	std::strncpy(pkg.desc_short, "DevilutionX", sizeof(pkg.desc_short) - 1);
	std::strncpy(pkg.desc_long, "DevilutionX Save Data", sizeof(pkg.desc_long) - 1);
	std::strncpy(pkg.app_id, "DevilutionX", sizeof(pkg.app_id) - 1);
	pkg.icon_cnt = 1;
	pkg.icon_anim_speed = 0;
	pkg.icon_data = g_blankIcon;
	pkg.eyecatch_type = VMUPKG_EC_NONE;

	std::string fullPath = std::string(vmuPath) + filename;

	// Delete existing file first (VMU filesystem requires this).
	fs_unlink(fullPath.c_str());

	file_t fd = fs_open(fullPath.c_str(), O_WRONLY);
	if (fd == FILEHND_INVALID) {
		LogError("[DC VMU] Cannot open {} for writing", fullPath);
		return false;
	}

	// Register VMU header with KOS so it wraps our payload on close.
	fs_vmu_set_header(fd, &pkg);

	ssize_t written = fs_write(fd, payload.get(), payloadSize);
	int closeRet = fs_close(fd);

	if (written < 0 || written != static_cast<ssize_t>(payloadSize)) {
		LogError("[DC VMU] Short write to {}: {} of {}", fullPath, written, payloadSize);
		return false;
	}
	if (closeRet < 0) {
		LogError("[DC VMU] Close failed for {} (VMU full?)", fullPath);
		return false;
	}

	LogVerbose("[DC VMU] Saved {} ({} -> {} bytes on VMU)", fullPath, size, payloadSize);
	return true;
}

std::unique_ptr<std::byte[]> ReadFromVmu(const char *vmuPath, const char *filename,
    size_t &outSize)
{
	outSize = 0;
	if (!InitLzo())
		return nullptr;

	std::string fullPath = std::string(vmuPath) + filename;

	file_t fd = fs_open(fullPath.c_str(), O_RDONLY);
	if (fd == FILEHND_INVALID)
		return nullptr;

	size_t total = fs_total(fd);
	if (total == static_cast<size_t>(-1) || total < sizeof(uint32_t)) {
		fs_close(fd);
		return nullptr;
	}

	auto rawBuf = std::make_unique<std::byte[]>(total);
	ssize_t bytesRead = fs_read(fd, rawBuf.get(), total);
	fs_close(fd);

	if (bytesRead < static_cast<ssize_t>(sizeof(uint32_t)))
		return nullptr;

	size_t available = static_cast<size_t>(bytesRead);

	// KOS transparently strips VMU package headers when present,
	// so fs_read returns just the payload regardless.
	//
	// The payload may be in one of these formats:
	//
	// Current (WriteCompressedFile-style, self-describing sizes):
	//   Compressed:   [4B compressedSize | 0x80000000][4B origSize][LZO data]
	//   Uncompressed: [4B dataSize (bit 31 clear)][raw data]
	//
	// Legacy (old WriteToVmu payload):
	//   Compressed:   [4B origSize > 0][LZO data]
	//   Uncompressed: [4B zero][raw data]

	uint32_t header;
	std::memcpy(&header, rawBuf.get(), sizeof(header));

	if (header & COMPRESSED_FLAG) {
		// Current format, compressed.
		size_t compressedSize = header & ~COMPRESSED_FLAG;
		if (available < sizeof(uint32_t) * 2 || compressedSize == 0)
			return nullptr;

		uint32_t originalSize;
		std::memcpy(&originalSize, rawBuf.get() + sizeof(uint32_t), sizeof(originalSize));
		if (originalSize == 0 || originalSize > MaxSaveDataSize)
			return nullptr;

		const std::byte *compData = rawBuf.get() + sizeof(uint32_t) * 2;
		size_t compAvail = available - sizeof(uint32_t) * 2;
		size_t srcLen = std::min(compressedSize, compAvail);

		auto result = std::make_unique<std::byte[]>(originalSize);
		lzo_uint decompLen = 0;
		int rc = lzo1x_decompress_safe(
		    reinterpret_cast<const unsigned char *>(compData),
		    static_cast<lzo_uint>(srcLen),
		    reinterpret_cast<unsigned char *>(result.get()),
		    &decompLen, nullptr);

		if (rc != LZO_E_OK || decompLen != originalSize) {
			LogError("[DC VMU] Decompression failed for {}: rc={} expected={} got={}",
			    fullPath, rc, originalSize, decompLen);
			return nullptr;
		}

		outSize = static_cast<size_t>(decompLen);
		LogVerbose("[DC VMU] Read {} ({} -> {} bytes)", fullPath, compressedSize, decompLen);
		return result;
	}

	// Bit 31 clear. Could be:
	//   - Legacy compressed: header = origSize > 0, rest is LZO data
	//   - Legacy uncompressed: header = 0, rest is raw data
	//   - Current uncompressed: header = dataSize > 0, rest is raw data

	if (header == 0) {
		// Legacy uncompressed sentinel. Return rest of data.
		size_t dataLen = available - sizeof(uint32_t);
		if (dataLen == 0)
			return nullptr;
		auto result = std::make_unique<std::byte[]>(dataLen);
		std::memcpy(result.get(), rawBuf.get() + sizeof(uint32_t), dataLen);
		outSize = dataLen;
		LogVerbose("[DC VMU] Read {} uncompressed ({} bytes)", fullPath, dataLen);
		return result;
	}

	if (header > MaxSaveDataSize)
		return nullptr;

	// header > 0 and reasonable. Try LZO decompression first (handles
	// legacy compressed where header = origSize). If that fails, treat
	// as current uncompressed where header = dataSize.
	const std::byte *afterHeader = rawBuf.get() + sizeof(uint32_t);
	size_t afterLen = available - sizeof(uint32_t);

	if (afterLen > 0) {
		auto tryBuf = std::make_unique<std::byte[]>(header);
		lzo_uint decompLen = 0;
		int rc = lzo1x_decompress_safe(
		    reinterpret_cast<const unsigned char *>(afterHeader),
		    static_cast<lzo_uint>(afterLen),
		    reinterpret_cast<unsigned char *>(tryBuf.get()),
		    &decompLen, nullptr);

		if ((rc == LZO_E_OK || rc == LZO_E_INPUT_NOT_CONSUMED)
		    && decompLen == header) {
			outSize = static_cast<size_t>(decompLen);
			LogVerbose("[DC VMU] Read {} (legacy compressed -> {} bytes)", fullPath, decompLen);
			return tryBuf;
		}
	}

	// Not compressed. Return header bytes of raw data.
	size_t dataLen = std::min(static_cast<size_t>(header), afterLen);
	if (dataLen == 0)
		return nullptr;
	auto result = std::make_unique<std::byte[]>(dataLen);
	std::memcpy(result.get(), afterHeader, dataLen);
	outSize = dataLen;
	LogVerbose("[DC VMU] Read {} uncompressed ({} bytes)", fullPath, dataLen);
	return result;
}

bool VmuFileExists(const char *vmuPath, const char *filename)
{
	std::string fullPath = std::string(vmuPath) + filename;
	file_t fd = fs_open(fullPath.c_str(), O_RDONLY);
	if (fd == FILEHND_INVALID)
		return false;
	fs_close(fd);
	return true;
}

} // namespace dc
} // namespace devilution

#endif // __DREAMCAST__
