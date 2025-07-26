float getLightArea(uint lightIndex) 
{
	ObjectDesc lightObjDesc = gObjectDescs.desc[gLightBuffer.lightIndex[lightIndex]];
	cumulativeTriangleAreaBuffer sum = cumulativeTriangleAreaBuffer(lightObjDesc.cumulativeTriangleAreaAddress);

	return sum.t[gLightBuffer.lights[lightIndex].triangleCount];
}

uint binarySearchTriangleIdx(const float lightArea, uint lightIndex, inout uint rngState) 
{
	ObjectDesc lightObjDesc = gObjectDescs.desc[gLightBuffer.lightIndex[lightIndex]];
	cumulativeTriangleAreaBuffer sum = cumulativeTriangleAreaBuffer(lightObjDesc.cumulativeTriangleAreaAddress);
	float target = random(rngState) * lightArea;

	uint l = 1;
	uint r = gLightBuffer.lights[lightIndex].triangleCount;
	while (l <= r) {
		uint mid = l + ((r - l) / 2);
		if (sum.t[mid - 1] < target && target <= sum.t[mid])
			return mid;
		else if (target > sum.t[mid]) l = mid + 1;
		else r = mid - 1;
	}
}

void uniformSamplePointOnTriangle(uint triangleIdx, 
								  uint lightIndex,
								  out vec3 pointOnTriangle, 
								  out vec3 normalOnTriangle, 
								  out vec3 pointOnTriangleWorld, 
								  out vec3 normalOnTriangleWorld, 
                                  inout uint rngState) 
{
	ObjectDesc lightObjDesc = gObjectDescs.desc[gLightBuffer.lightIndex[lightIndex]];

	IndexBuffer lightIndexBuffer = IndexBuffer(lightObjDesc.indexDeviceAddress);
	uint base = triangleIdx * 3u;
	uvec3 index = uvec3(lightIndexBuffer.i[base + 0],
                        lightIndexBuffer.i[base + 1],
	                    lightIndexBuffer.i[base + 2]);

	PositionBuffer lightPositionBuffer = PositionBuffer(lightObjDesc.vertexPositionDeviceAddress);
	vec4 p0 = lightPositionBuffer.p[index.x];
    vec4 p1 = lightPositionBuffer.p[index.y];
    vec4 p2 = lightPositionBuffer.p[index.z];

	AttributeBuffer attributeBuffer = AttributeBuffer(lightObjDesc.vertexAttributeDeviceAddress);
	VertexAttributes a0 = attributeBuffer.a[index.x];
	VertexAttributes a1 = attributeBuffer.a[index.y];
	VertexAttributes a2 = attributeBuffer.a[index.z];
	vec3 n0 = normalize(a0.norm.xyz);
	vec3 n1 = normalize(a1.norm.xyz);
 	vec3 n2 = normalize(a2.norm.xyz);

	vec2 xi   = vec2(random(rngState), random(rngState));
	float su0 = sqrt(xi.x); // total needs to be 1
	float u  = 1.0 - su0;
	float v  = xi.y * su0;
	float w  = 1.0 - u - v;

	pointOnTriangle = (w * p0 + u * p1 + v * p2).xyz;
	normalOnTriangle = normalize(w * n0 + u * n1 + v * n2).xyz;

    mat4 localToWorld = transpose(gLightBuffer.lights[lightIndex].transform);
    pointOnTriangleWorld = (localToWorld * vec4(pointOnTriangle, 1.0f)).xyz;
	normalOnTriangleWorld = normalize(localToWorld * vec4(normalOnTriangle, 0.0f)).xyz;
}

uint selectLight(vec3 worldPos, inout uint rngState)
{
	uint gridDimension = 8;
	float cellExtent = 0.2f;
	float cellSize = cellExtent * 2;
	float gridExtent = -cellSize * gridDimension / 2;
	vec3 gridPosMin = vec3(gridExtent, gridExtent, gridExtent) / cellSize;
	vec3 gridPosMax = -gridPosMin;
	vec3 gridPos = clamp(worldPos / cellSize, gridPosMin, gridPosMax) - gridPosMin;
	ivec3 gridCoords = ivec3(floor(gridPos));
	vec3 cellFrac = fract(gridPos);

	uint gridIndex = gridCoords.x + gridCoords.y * gridDimension + gridCoords.z * gridDimension * gridDimension;
	LightGrid grid = gLightBuffer.grid[gridIndex];

	uint randomIndex = pcg_hash(rngState) % 256;
    return grid.lightIndex[randomIndex];
}
