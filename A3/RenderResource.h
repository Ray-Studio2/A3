#pragma once

#include <memory>

namespace A3
{
struct IAccelerationStructure
{
	virtual ~IAccelerationStructure() {}
};

using IAccelerationStructureRef = std::unique_ptr<IAccelerationStructure>;
}