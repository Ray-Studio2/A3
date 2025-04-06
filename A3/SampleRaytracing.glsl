#extension GL_EXT_ray_tracing : enable

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

#if BACKGROUND_MISS_SHADER
//=========================
//	BACKGROUND MISS SHADER
//=========================
layout( location = 0 ) rayPayloadInEXT vec3 hitValue;

void main()
{
	hitValue = vec3( 0.0, 0.0, 0.2 );
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

layout( std430, binding = 3 ) buffer VertexPosition
{
	vec4 vPosBuffer[];
};

layout( std430, binding = 4 ) buffer VertexAttribute
{
	VertexAttributes vAttribBuffer[];
};

layout( std430, binding = 5 ) buffer Indices
{
	uint idxBuffer[];
};

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
	hitValue = color;
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
