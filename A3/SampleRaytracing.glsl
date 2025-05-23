#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#if RAY_GENERATION_SHADER
//=========================
//	RAY GENERATION SHADER
//=========================
layout( binding = 0 ) uniform accelerationStructureEXT topLevelAS;
layout( binding = 1, rgba8 ) uniform image2D image;
layout( binding = 2 ) uniform CameraProperties
{
	vec3 cameraPos;
	float yFov_degree;
} g;

layout( location = 0 ) rayPayloadEXT vec3 hitValue;

void main()
{
	const vec3 cameraX = vec3( 1, 0, 0 );
	const vec3 cameraY = vec3( 0, -1, 0 );
	const vec3 cameraZ = vec3( 0, 0, -1 );
	const float aspect_y = tan( radians( g.yFov_degree ) * 0.5 );
	const float aspect_x = aspect_y * float( gl_LaunchSizeEXT.x ) / float( gl_LaunchSizeEXT.y );

	const vec2 screenCoord = vec2( gl_LaunchIDEXT.xy ) + vec2( 0.5 );
	const vec2 ndc = screenCoord / vec2( gl_LaunchSizeEXT.xy ) * 2.0 - 1.0;
	vec3 rayDir = ndc.x * aspect_x * cameraX + ndc.y * aspect_y * cameraY + cameraZ;

	hitValue = vec3( 0.0 );

	traceRayEXT(
		topLevelAS,                         // topLevel
		gl_RayFlagsOpaqueEXT, 0xff,         // rayFlags, cullMask
		0, 1, 0,                            // sbtRecordOffset, sbtRecordStride, missIndex
		g.cameraPos, 0.0, rayDir, 100.0,    // origin, tmin, direction, tmax
		0 );                                 // payload

	imageStore( image, ivec2( gl_LaunchIDEXT.xy ), vec4( hitValue, 0.0 ) );
}
#endif

#if CLOSEST_HIT_SHADER
//=========================
//	CLOSEST HIT SHADER
//=========================
struct VertexAttributes
{
	vec4 norm;
	vec4 uv;
};

struct ObjectDesc
{
	uint64_t vertexPositionDeviceAddress;
	uint64_t vertexAttributeDeviceAddress;
	uint64_t indexDeviceAddress;
};

layout(buffer_reference, scalar) buffer PositionBuffer { vec4 p[]; };
layout(buffer_reference, scalar) buffer AttributeBuffer { VertexAttributes a[]; };
layout(buffer_reference, scalar) buffer IndexBuffer { ivec3 i[]; }; // Triangle indices
layout( binding = 3, scalar) buffer ObjectDescBuffer
{
	ObjectDesc desc[];
} objectDescs;

layout( shaderRecordEXT ) buffer CustomData
{
	vec3 color;
};

layout( binding = 0 ) uniform accelerationStructureEXT topLevelAS;

layout( location = 0 ) rayPayloadInEXT vec3 hitValue;
layout( location = 1 ) rayPayloadEXT uint missCount;
hitAttributeEXT vec2 attribs;

void main()
{
	ObjectDesc objDesc = objectDescs.desc[gl_InstanceCustomIndexEXT];

	IndexBuffer indexBuffer = IndexBuffer(objDesc.indexDeviceAddress);
	ivec3 index = indexBuffer.i[gl_PrimitiveID];
	
	PositionBuffer positionBuffer = PositionBuffer(objDesc.vertexPositionDeviceAddress);
	vec4 p0 = positionBuffer.p[index.x];
    vec4 p1 = positionBuffer.p[index.y];
    vec4 p2 = positionBuffer.p[index.z];
	
	float u = attribs.x;
	float v = attribs.y;
	float w = 1.0 - u - v;
	
	vec3 position = (w * p0 + u * p1 + v * p2).xyz;

	AttributeBuffer attributeBuffer = AttributeBuffer(objDesc.vertexAttributeDeviceAddress);
	VertexAttributes vertexAttributes0 = attributeBuffer.a[index.x];
	VertexAttributes vertexAttributes1 = attributeBuffer.a[index.y];
	VertexAttributes vertexAttributes2 = attributeBuffer.a[index.z];
	vec3 n0 = normalize(vertexAttributes0.norm.xyz);
	vec3 n1 = normalize(vertexAttributes1.norm.xyz);
 	vec3 n2 = normalize(vertexAttributes2.norm.xyz);
	vec3 localNormal = normalize(w * n0 + u * n1 + v * n2);

	mat4x3 objToWorld = gl_ObjectToWorldEXT;
	vec3 worldNormal = normalize(objToWorld * vec4(localNormal, 0.0));
	vec3 worldPos = objToWorld * vec4(position, 1.0);

	vec3 lightPos = vec3(0.0, 0.0, 10.0);
	vec3 lightDir = normalize(lightPos - worldPos);
	float diff = max(dot(worldNormal, lightDir), 0.0);

	hitValue = vec3(diff) * color;
}

#endif

#if SHADOW_MISS_SHADER
//=========================
//	SHADOW MISS SHADER
//=========================
layout( location = 1 ) rayPayloadInEXT uint missCount;

void main()
{

}
#endif


#if ENVIRONMENT_MISS_SHADER
//=========================
//	ENVIRONMENT MISS SHADER
//=========================
layout( location = 0 ) rayPayloadInEXT vec3 hitValue;

void main()
{
	hitValue = vec3( 0.0, 0.0, 0.2 );
}
#endif