#pragma once

#include "RenderResource.h"
#include <vector>

namespace A3
{
struct BLASBatch
{
	IAccelerationStructureRef blas;

	std::vector<Mat4x4> transforms;
};
}