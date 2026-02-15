/**
 * @file dc_streaming.h
 * @brief Dreamcast Asset Streaming System
 */
#pragma once

#ifdef __DREAMCAST__

#include <cstddef>
#include <cstdint>
#include <string>

namespace devilution {
namespace dc {

// Initialization
void Streaming_Init(int numChannels);
void Streaming_Shutdown();

// Requesting assets
// If the asset is loaded, returns true immediately.
// If not, queues it for loading and returns false.
bool Streaming_RequestAsset(const std::string &filename, int flags);

// Checking status
bool Streaming_IsAssetLoaded(const std::string &filename);

// Retrieving data
// Returns pointer to data if loaded, nullptr otherwise.
const uint8_t *Streaming_GetData(const std::string &filename, size_t *outSize);

// Maintenance (call once per frame)
void Streaming_Update();

// Loading Screen helper (blocks until priority requests are done)
void Streaming_LoadAllRequestedAssets(bool priorityOnly);

} // namespace dc
} // namespace devilution

#endif // __DREAMCAST__
