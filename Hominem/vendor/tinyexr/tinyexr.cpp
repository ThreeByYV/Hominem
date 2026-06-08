// Single implementation TU for tinyexr (mirrors vendor/stb_image/stb_image.cpp).
// Use the project's existing zlib (assimp's zlibstatic) instead of bundled miniz
// so we don't get duplicate-symbol clashes (compress/uncompress/crc32/...).
#define TINYEXR_IMPLEMENTATION
#define TINYEXR_USE_MINIZ 0
#include <zlib.h>
#include "tinyexr.h"
