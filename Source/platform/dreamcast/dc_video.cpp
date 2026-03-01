/**
 * @file dc_video.cpp
 * @brief Dreamcast video conversion implementation
 *
 * The "Inner Loop" - this code runs 307,200 times per frame (640x480).
 * Every cycle counts!
 *
 * Optimization techniques used:
 * 1. Palette LUT fits in L1 cache (512 bytes for 256 RGB565 entries)
 * 2. Process 16 pixels at a time for throughput
 */

#ifdef __DREAMCAST__

#include "dc_video.h"

#include <kos.h>

namespace devilution {
namespace dc {

namespace {

// RGB565 palette lookup table (256 entries x 2 bytes = 512 bytes)
// Aligned to 32 bytes for cache efficiency
alignas(32) uint16_t palette565[256];

bool initialized = false;

/**
 * @brief Convert 16 pixels from 8bpp to 16bpp
 *
 * This is the innermost loop - fully unrolled for speed.
 */
inline void Convert16Pixels(const uint8_t *src, uint16_t *dst)
{
	dst[0] = palette565[src[0]];
	dst[1] = palette565[src[1]];
	dst[2] = palette565[src[2]];
	dst[3] = palette565[src[3]];
	dst[4] = palette565[src[4]];
	dst[5] = palette565[src[5]];
	dst[6] = palette565[src[6]];
	dst[7] = palette565[src[7]];
	dst[8] = palette565[src[8]];
	dst[9] = palette565[src[9]];
	dst[10] = palette565[src[10]];
	dst[11] = palette565[src[11]];
	dst[12] = palette565[src[12]];
	dst[13] = palette565[src[13]];
	dst[14] = palette565[src[14]];
	dst[15] = palette565[src[15]];
}

void ConvertFrame(const uint8_t *src, uint16_t *dst, int width, int height, int srcPitch, int dstPitch)
{
	for (int y = 0; y < height; y++) {
		const uint8_t *srcRow = src + y * srcPitch;
		uint16_t *dstRow = reinterpret_cast<uint16_t *>(reinterpret_cast<uint8_t *>(dst) + y * dstPitch);

		int x = 0;
		for (; x + 16 <= width; x += 16) {
			Convert16Pixels(srcRow + x, dstRow + x);
		}

		for (; x < width; x++) {
			dstRow[x] = palette565[srcRow[x]];
		}
	}
}

} // anonymous namespace

bool VideoInit([[maybe_unused]] int width, [[maybe_unused]] int height)
{
	for (int i = 0; i < 256; i++) {
		palette565[i] = RGB888toRGB565(i, i, i);
	}

	initialized = true;
	return true;
}

void VideoShutdown()
{
	initialized = false;
}

void UpdatePalette(const SDL_Palette *palette)
{
	if (!palette || !palette->colors)
		return;

	UpdatePaletteRange(palette->colors, 0, palette->ncolors);
}

void UpdatePaletteRange(const SDL_Color *colors, int firstColor, int nColors)
{
	if (!colors)
		return;

	if (firstColor + nColors > 256)
		nColors = 256 - firstColor;

	for (int i = 0; i < nColors; i++) {
		const SDL_Color &c = colors[i];
		palette565[firstColor + i] = RGB888toRGB565(c.r, c.g, c.b);
	}
}

void ConvertAndUpload(const SDL_Surface *src, SDL_Surface *dst)
{
	if (!initialized || !src || !dst)
		return;

	const uint8_t *srcPixels = static_cast<const uint8_t *>(src->pixels);
	uint16_t *dstPixels = static_cast<uint16_t *>(dst->pixels);

	if (!srcPixels || !dstPixels)
		return;

	const int width = src->w < dst->w ? src->w : dst->w;
	const int height = src->h < dst->h ? src->h : dst->h;

	ConvertFrame(srcPixels, dstPixels, width, height, src->pitch, dst->pitch);
}

bool IsInitialized()
{
	return initialized;
}

} // namespace dc
} // namespace devilution

#endif // __DREAMCAST__
