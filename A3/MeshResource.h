#pragma once

#include <vector>
#include "EngineTypes.h"

namespace A3
{
struct VertexPosition
{
    float x, y, z, w;

    VertexPosition operator - (const VertexPosition& other) const {
        return { x - other.x, y - other.y, z - other.z, 0 };
    }

    VertexPosition cross(const VertexPosition& other) const {
        return {
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        };
    }

    float length() const {
        return sqrt(x * x + y * y + z * z);
    }

    /// 벡터 * 스칼라
    VertexPosition operator*(float s) const {
        return { x * s, y * s, z * s, w * s };
    }

    /// 스칼라 * 벡터  (비멤버·friend 로 정의)
    friend VertexPosition operator*(float s, const VertexPosition& v) {
        return v * s;          // 위 멤버 함수를 재사용
    }
};

struct VertexAttributes
{
    float normals[ 4 ];
    float uvs[ 4 ];
};

struct MeshResource
{
    std::vector<VertexPosition> positions;
    std::vector<VertexAttributes> attributes;
    std::vector<uint32> indices;
    std::vector<float> cumulativeTriangleArea;
    uint32 triangleCount;
};
}