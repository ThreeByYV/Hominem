#pragma once

#include <iostream>
#include <memory>
#include <utility>
#include <algorithm>
#include <functional>
#include <cmath>
#include <cstdio>

#include <array>
#include <expected>
#include <span>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <Hominem/Core/Log.h>
#include <filesystem>
#include <cstdint>


// Vendor headers included in every TU — compiled once via PCH
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#ifdef HMN_PLATFORM_WINDOWS
	#include "Hominem/Core/Window.h"
#endif

#include "Hominem/Core/Profiler.h"
#include "Hominem/Core/Units.h"