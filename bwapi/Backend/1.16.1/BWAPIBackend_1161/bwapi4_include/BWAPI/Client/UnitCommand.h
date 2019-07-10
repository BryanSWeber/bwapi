#pragma once

#include <BWAPI/UnitCommandType.h>

/**
 * UnitOrder contains a single whole order
 */

namespace BWAPIC
{
  struct UnitCommand
  {
    BWAPI::UnitCommandType type;
    int unitIndex;
    int targetIndex;
    int x;
    int y;
    int extra;
  };
}