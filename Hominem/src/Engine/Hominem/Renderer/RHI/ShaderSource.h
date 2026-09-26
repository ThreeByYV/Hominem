#pragma once

#include "Hominem/Core/Core.h"
#include <string>

namespace Hominem {

class ShaderSource : public RefCounted
{
public:
    explicit ShaderSource(std::string src) : source(std::move(src)) {}

    std::string source;
};

}
