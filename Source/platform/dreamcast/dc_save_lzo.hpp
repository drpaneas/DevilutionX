/**
 * @file dc_save_lzo.hpp
 * @brief LZO compression for Dreamcast VMU saves
 *
 * Provides compression/decompression functions for save data to fit
 * within VMU's limited ~100KB capacity.
 */

#pragma once

#ifdef __DREAMCAST__

#include <cstddef>
#include <cstdint>
#include <memory>

namespace devilution {
namespace dc {

/**
 * Initialize the LZO library.
 * Called automatically by compress/decompress functions.
 * @return true if initialization succeeded
 */
bool InitLzo();

/**
 * Compress data using LZO algorithm.
 * @param data Input data to compress
 * @param size Size of input data in bytes
 * @param outSize Output: size of compressed data (0 if compression didn't help)
 * @return Compressed data, or nullptr if compression failed/didn't reduce size
 */
std::unique_ptr<std::byte[]> CompressData(const std::byte *data, size_t size, size_t &outSize);

/**
 * Decompress LZO-compressed data.
 * @param compressedData Compressed input data
 * @param compressedSize Size of compressed data
 * @param expectedSize Expected size of decompressed data
 * @param outSize Output: actual decompressed size
 * @return Decompressed data, or nullptr on failure
 */
std::unique_ptr<std::byte[]> DecompressData(const std::byte *compressedData, size_t compressedSize,
    size_t expectedSize, size_t &outSize);

/**
 * Write data to a file with LZO compression.
 * Compressed format: [4B header|0x80000000][4B original size][compressed data]
 * Uncompressed format: [4B size][raw data]
 * @param path File path to write to
 * @param data Data to write
 * @param size Size of data
 * @return true on success
 */
bool WriteCompressedFile(const char *path, const std::byte *data, size_t size);

/**
 * Read and decompress data from a file.
 * @param path File path to read from
 * @param outSize Output: size of decompressed data
 * @return Decompressed data, or nullptr on failure
 */
std::unique_ptr<std::byte[]> ReadCompressedFile(const char *path, size_t &outSize);

/**
 * Write data to VMU with proper VMU package headers.
 * The data is LZO-compressed and wrapped in a vmu_pkg_t so that
 * the VMU BIOS recognizes the file and preserves its exact size.
 * @param vmuPath VMU base path (e.g. "/vmu/a1/")
 * @param filename File name within VMU (max 12 chars)
 * @param data Data to write
 * @param size Size of data
 * @return true on success
 */
bool WriteToVmu(const char *vmuPath, const char *filename,
    const std::byte *data, size_t size);

/**
 * Read data from VMU, unwrapping the VMU package header and
 * decompressing the LZO payload.
 * @param vmuPath VMU base path (e.g. "/vmu/a1/")
 * @param filename File name within VMU (max 12 chars)
 * @param outSize Output: size of decompressed data
 * @return Decompressed data, or nullptr on failure
 */
std::unique_ptr<std::byte[]> ReadFromVmu(const char *vmuPath, const char *filename,
    size_t &outSize);

/**
 * Check whether a file exists on VMU.
 * @param vmuPath VMU base path (e.g. "/vmu/a1/")
 * @param filename File name within VMU
 * @return true if the file exists
 */
bool VmuFileExists(const char *vmuPath, const char *filename);

} // namespace dc
} // namespace devilution

#endif // __DREAMCAST__
