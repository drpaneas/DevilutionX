#ifdef __DREAMCAST__

#include "dc_init.hpp"
#include "dc_video.h"
#include "utils/paths.h"
#include <cstdio>
#include <dc/maple.h>
#include <dc/maple/vmu.h>
#include <kos.h>

namespace devilution {
namespace dc {

static bool g_vmuAvailable = false;
static char g_vmuPath[32] = "/vmu/a1/";

static bool CheckVmuAvailable()
{
	maple_device_t *vmu = maple_enum_type(0, MAPLE_FUNC_MEMCARD);
	if (vmu) {
		snprintf(g_vmuPath, sizeof(g_vmuPath), "/vmu/%c%d/",
		    'a' + vmu->port, vmu->unit);
		g_vmuAvailable = true;
		return true;
	}
	g_vmuAvailable = false;
	return false;
}

bool IsVmuAvailable()
{
	return g_vmuAvailable;
}

const char *GetVmuPath()
{
	return g_vmuPath;
}

bool InitDreamcast()
{
	paths::SetBasePath("/cd/");
	CheckVmuAvailable();
	// Use /ram/ for saves until VMU package headers are implemented.
	// KOS VMUFS requires vmu_pkg_build() wrapping which dc_save_lzo
	// doesn't do yet, so raw writes corrupt the roundtrip.
	paths::SetPrefPath("/ram/");
	paths::SetConfigPath("/ram/");

	return true;
}

void ShutdownDreamcast()
{
}

} // namespace dc
} // namespace devilution

#endif
