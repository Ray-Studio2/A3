#pragma once

#include "RenderResource.h"
#include <vector>

namespace A3
{
struct BLASBatch
{
	IAccelerationStructureRef blas;

	std::vector<Mat3x4> transforms;
};
}