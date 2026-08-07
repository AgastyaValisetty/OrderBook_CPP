#pragma once

#include "LevelInfo.h"
#include "Usings.h"

struct LevelInfo {
    Price price;
    Quantity quantity_;
};
using LevelInfos = std::vector<LevelInfos>;