#pragma once

#include <string>
#include <vector>
#include "Vector.h"
#include "Matrix.h"
#include "EngineTypes.h"

namespace A3
{
struct VertexPosition : public Vec4
{
    using Vec4::Vec4;

    VertexPosition operator - (const VertexPosition& other) const {
        return { x - other.x, y - other.y, z - other.z, 0.0f };
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

inline VertexPosition operator*(const Mat4x4& m, const VertexPosition& v)
{
    return {
    m.m00 * v.x + m.m01 * v.y + m.m02 * v.z + m.m03 * v.w,
    m.m10 * v.x + m.m11 * v.y + m.m12 * v.z + m.m13 * v.w,
    m.m20 * v.x + m.m21 * v.y + m.m22 * v.z + m.m23 * v.w,
    m.m30 * v.x + m.m31 * v.y + m.m32 * v.z + m.m33 * v.w
    };
}

inline VertexPosition cross(const VertexPosition& lhs, const VertexPosition& rhs) {
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x,
        0.0f
    };
}

inline float dot(const VertexPosition& lhs, const VertexPosition& rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

struct VertexAttributes
{
    float normals[ 4 ];
    float uvs[ 4 ];
};

struct Bone
{
    std::string _bonaName;
    int _parentBoneIndex = -1;
    std::vector<int> _childBoneIndexArr;
};
struct Skeleton
{
    std::string _skinName;
    std::vector<Bone> _boneArray;
    std::vector<Mat4x4> _boneDressPoseArray;
    std::vector<Mat4x4> _boneDressPoseInverseArray;
};

struct MeshResource
{
    std::vector<VertexPosition> positions;
    std::vector<VertexAttributes> attributes;
    std::vector<uint32> indices;
    std::vector<float> cumulativeTriangleArea;
    uint32 triangleCount;

    Skeleton _skeleton;
};
}