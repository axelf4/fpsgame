#include "renderer.h"
#include <stdlib.h>
#include <math.h>
#include "glUtil.h"
#include "ddsloader.h"

#define DEGREES_TO_RADIANS(a) ((a) * M_PI / 180)
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define DEPTH_SIZE 16384
#define Z_NEAR (1.0f)
#define Z_FAR (100.0f)
#define FOV (90.0f)
#define NUM_FRUSTUM_CORNERS 8
#define RENDER_MASK (POSITION_COMPONENT_MASK | MODEL_COMPONENT_MASK)

struct Frustum {
	float neard;
	float fard;
	// In radians
	float fov;
	float ratio;
};

static MATRIX lookAt(VECTOR eye, VECTOR center, VECTOR up) {
	ALIGN(16) float eyeV[4], centerV[4], upV[4];
	VectorGet(eyeV, eye);
	VectorGet(centerV, center);
	VectorGet(upV, up);

	float z0 = eyeV[0] - centerV[0],
	z1 = eyeV[1] - centerV[1],
	z2 = eyeV[2] - centerV[2];

	float len = 1 / sqrt(z0 * z0 + z1 * z1 + z2 * z2);
	z0 *= len;
	z1 *= len;
	z2 *= len;

	float x0 = upV[1] * z2 - upV[2] * z1,
		  x1 = upV[2] * z0 - upV[0] * z2,
		  x2 = upV[0] * z1 - upV[1] * z0;
	len = sqrt(x0 * x0 + x1 * x1 + x2 * x2);
	if (len) {
		len = 1 / len;
		x0 *= len;
		x1 *= len;
		x2 *= len;
	} else {
		x0 = x1 = x2 = 0;
	}

	float y0 = z1 * x2 - z2 * x1,
		  y1 = z2 * x0 - z0 * x2,
		  y2 = z0 * x1 - z1 * x0;
	len = sqrt(y0 * y0 + y1 * y1 + y2 * y2);
	if (len) {
		len = 1 / len;
		y0 *= len;
		y1 *= len;
		y2 *= len;
	} else {
		y0 = y1 = y2 = 0;
	}

	return MatrixSet(x0, y0, z0, 0,
			x1, y1, z1, 0,
			x2, y2, z2, 0,
			-(x0 * eyeV[0] + x1 * eyeV[1] + x2 * eyeV[2]), -(y0 * eyeV[0] + y1 * eyeV[1] + y2 * eyeV[2]), -(z0 * eyeV[0] + z1 * eyeV[1] + z2 * eyeV[2]), 1);
}

/**
 * Computes the near and far distances for every frustum slice in camera eye space.
 * @param splitDistances Distances are output into this array
 */
static void getSplitDistances(float *splitDistances, float near, float far) {
	const float lambda = 0.75f, // Split weight
		  ratio = far / near;
	splitDistances[0] = near;
	for(int i = 1; i < NUM_SPLITS; ++i) {
		const float si = i / (float) NUM_SPLITS;
		splitDistances[i] = lambda * (near * powf(ratio, si)) + (1 - lambda) * (near + (far - near) * si);
	}
	splitDistances[NUM_SPLITS] = far;
}

/**
 * Computes the 8 corners of the current view frustum.
 * @param f The view frustum.
 * @param center The position of the camera.
 * @param viewDir The normalized direction the camera is looking.
 * @param up The normalized up vector.
 * @param points Gets set to the corners of the frustum.
 */
static void getFrustumPoints(struct Frustum f, VECTOR center, VECTOR viewDir, VECTOR up, VECTOR *points) {
	// VECTOR up = VectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	VECTOR right = Vector4Normalize(VectorCross(viewDir, up));
	const VECTOR fc = VectorAdd(center, VectorMultiply(viewDir, VectorReplicate(f.fard))),
		  nc = VectorAdd(center, VectorMultiply(viewDir, VectorReplicate(f.neard)));
	// Half the heights and widths of the near and far plane rectangles
	const float nearHeight = tan(0.5f * f.fov) * f.neard,
		  nearWidth = nearHeight * f.ratio,
		  farHeight = tan(0.5f * f.fov) * f.fard,
		  farWidth = farHeight * f.ratio;

	points[0] = VectorSubtract(VectorSubtract(nc, VectorMultiply(up, VectorReplicate(nearHeight))), VectorMultiply(right, VectorReplicate(nearWidth)));
	points[1] = VectorSubtract(VectorAdd(nc, VectorMultiply(up, VectorReplicate(nearHeight))), VectorMultiply(right, VectorReplicate(nearWidth)));
	points[2] = VectorAdd(VectorAdd(nc, VectorMultiply(up, VectorReplicate(nearHeight))), VectorMultiply(right, VectorReplicate(nearWidth)));
	points[3] = VectorAdd(VectorSubtract(nc, VectorMultiply(up, VectorReplicate(nearHeight))), VectorMultiply(right, VectorReplicate(nearWidth)));

	points[4] = VectorSubtract(VectorSubtract(fc, VectorMultiply(up, VectorReplicate(farHeight))), VectorMultiply(right, VectorReplicate(farWidth)));
	points[5] = VectorSubtract(VectorAdd(fc, VectorMultiply(up, VectorReplicate(farHeight))), VectorMultiply(right, VectorReplicate(farWidth)));
	points[6] = VectorAdd(VectorAdd(fc, VectorMultiply(up, VectorReplicate(farHeight))), VectorMultiply(right, VectorReplicate(farWidth)));
	points[7] = VectorAdd(VectorSubtract(fc, VectorMultiply(up, VectorReplicate(farHeight))), VectorMultiply(right, VectorReplicate(farWidth)));
}

/**
 * Builds a matrix for cropping the light's projection.
 * @param points Frustum corners
 */
static float calculateCropMatrix(struct Frustum f, VECTOR *points, MATRIX lightView, MATRIX *shadowCPM) {
	ALIGN(16) float vv[4];
	float maxZ = -INFINITY, minZ = INFINITY;
	for (int i = 0; i < 8; ++i) {
		const VECTOR transf = VectorTransform(points[i], lightView);
		VectorGet(vv, transf);
		if (vv[2] < minZ) minZ = vv[2];
		if (vv[2] > maxZ) maxZ = vv[2];
	}

	const MATRIX shadProj = MatrixOrtho(-1.0f, 1.0f, -1.0f, 1.0f, -maxZ, -minZ), // Set the projection matrix with the new z-bounds
		  lightViewProjection = MatrixMultiply(shadProj, lightView);

	// Find extents of frustum slice in light's homogeneous coordinates
	float minX = INFINITY, maxX = -INFINITY, minY = INFINITY, maxY = -INFINITY;
	for (int i = 0; i < 8; ++i) {
		const VECTOR v = VectorTransform(points[i], lightViewProjection);
		VectorGet(vv, v);
		vv[0] /= vv[3];
		vv[1] /= vv[3];
		if (vv[0] < minX) minX = vv[0];
		if (vv[0] > maxX) maxX = vv[0];
		if (vv[1] < minY) minY = vv[1];
		if (vv[1] > maxY) maxY = vv[1];
	}

	const float scaleX = 2.0f / (maxX - minX),
		  scaleY = 2.0f / (maxY - minY),
		  offsetX = -0.5f * (maxX + minX) * scaleX,
		  offsetY = -0.5f * (maxY + minY) * scaleY;
	const MATRIX cropMatrix = MatrixSet(
			scaleX, 0.0f, 0.0f, 0.0f,
			0.0f, scaleY, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			offsetX, offsetY, 0.0f, 1.0f);
	*shadowCPM = MatrixMultiply(cropMatrix, lightViewProjection);

	return minZ;
}

static void drawModelGeometry(struct Model *model, const GLuint attrib) {
	glBindBuffer(GL_ARRAY_BUFFER, model->vertexBuffer);
	glVertexAttribPointer(attrib, 3, GL_FLOAT, GL_FALSE, model->stride, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model->indexBuffer);
	for (int i = 0; i < model->numParts; ++i) {
		struct ModelPart *part = model->parts + i;
		glDrawElements(GL_TRIANGLES, part->count, GL_UNSIGNED_INT, (const GLvoid *) (uintptr_t) part->offset);
	}
}

static void drawModelGeometryNormal(struct Model *model, const GLuint posAttrib, const GLuint normalAttrib) {
	glBindBuffer(GL_ARRAY_BUFFER, model->vertexBuffer);
	glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, model->stride, 0);
	glVertexAttribPointer(normalAttrib, 3, GL_FLOAT, GL_FALSE, model->stride, (const GLvoid *) (sizeof(GLfloat) * 3));
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model->indexBuffer);
	for (int i = 0; i < model->numParts; ++i) {
		struct ModelPart *part = model->parts + i;
		glDrawElements(GL_TRIANGLES, part->count, GL_UNSIGNED_INT, (const GLvoid *) (uintptr_t) part->offset);
	}
}

void rendererResize(struct Renderer *renderer, int width, int height) {
	ALIGN(16) float mv[16];
	renderer->width = width;
	renderer->height = height;
	renderer->projection = MatrixPerspective(FOV, (float) width / height, Z_NEAR, Z_FAR);

	glUseProgram(renderer->ssaoProgram);
	const float projScale = width / (-2.0f * tanf(DEGREES_TO_RADIANS(0.5f * FOV)));
	glUniform1f(glGetUniformLocation(renderer->ssaoProgram, "projScale"), projScale);
	MatrixGet(mv, renderer->projection);
	glUniform4f(glGetUniformLocation(renderer->ssaoProgram, "projInfo"), 2.0f / (width * mv[0]), 2.0f / (height * mv[5]), -1.0f / mv[0], -1.0f / mv[5]);
	glUniform2f(glGetUniformLocation(renderer->ssaoProgram, "invResolution"), 1.0f / width, 1.0f / height);

	glBindTexture(GL_TEXTURE_2D, renderer->depthTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
	glBindTexture(GL_TEXTURE_2D, renderer->sceneTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glBindTexture(GL_TEXTURE_2D, renderer->ssaoTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glBindTexture(GL_TEXTURE_2D, renderer->blurTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
}

int rendererInit(struct Renderer *renderer, struct EntityManager *manager, int width, int height) {
	ALIGN(16) float vv[4], mv[16];
	renderer->manager = manager;
	renderer->width = width;
	renderer->height = height;
	const GLchar *vertexShaderSource = "attribute vec3 position;"
		"attribute vec3 normal;"
		"uniform mat4 mvp;"
		"uniform mat4 model;"
		"uniform vec3 lightDir;"
		"const int NUM_CASCADES = 3;"
		"uniform mat4 lightMVP[NUM_CASCADES];"
		"varying vec4 lightSpacePos[NUM_CASCADES];"
		"varying vec3 vNormal;"
		"void main() {"
		"	vNormal = vec3(model * vec4(normal, 0.0));"
		"	gl_Position = mvp * vec4(position, 1.0);"
		"	for (int i = 0; i < NUM_CASCADES; ++i) {"
		"		lightSpacePos[i] = lightMVP[i] * vec4(position, 1.0);"
		"	}"
		"}",
		*fragmentShaderSource = "#extension GL_EXT_gpu_shader4 : enable\n"
			"#ifdef GL_ES\n"
			"precision highp float;\n"
			"#endif\n"
			"varying vec3 vNormal;"
			"uniform vec3 lightDir;"
			"const int NUM_CASCADES = 3;"
			"varying vec4 lightSpacePos[NUM_CASCADES];"
			"uniform float cascadeEndClipSpace[NUM_CASCADES];\n"
			"#ifndef GL_EXT_gpu_shader4\n"
			"uniform sampler2D shadowMap[NUM_CASCADES];\n"
			"#else\n"
			"uniform sampler2DShadow shadowMap[NUM_CASCADES];"
			"vec2 depthGradient(vec2 uv, float z) {" // Receiver plane depth bias
			"	vec3 duvdist_dx = dFdx(vec3(uv, z)), duvdist_dy = dFdy(vec3(uv, z));"
			"	vec2 biasUV;" // dz_duv
			"	biasUV.x = duvdist_dy.y * duvdist_dx.z - duvdist_dx.y * duvdist_dy.z;"
			"	biasUV.y = duvdist_dx.x * duvdist_dy.z - duvdist_dy.x * duvdist_dx.z;"
			"	biasUV /= (duvdist_dx.x * duvdist_dy.y - duvdist_dx.y * duvdist_dy.x);"
			"	return biasUV;"
			"}\n"
			"#endif\n"
			"float calcShadowFactor() {"
			"	vec2 poisson[25];"
			"	poisson[0] = vec2(-0.978698, -0.0884121);"
			"	poisson[1] = vec2(-0.841121, 0.521165);"
			"	poisson[2] = vec2(-0.71746, -0.50322);"
			"	poisson[3] = vec2(-0.702933, 0.903134);"
			"	poisson[4] = vec2(-0.663198, 0.15482);"
			"	poisson[5] = vec2(-0.495102, -0.232887);"
			"	poisson[6] = vec2(-0.364238, -0.961791);"
			"	poisson[7] = vec2(-0.345866, -0.564379);"
			"	poisson[8] = vec2(-0.325663, 0.64037);"
			"	poisson[9] = vec2(-0.182714, 0.321329);"
			"	poisson[10] = vec2(-0.142613, -0.0227363);"
			"	poisson[11] = vec2(-0.0564287, -0.36729);"
			"	poisson[12] = vec2(-0.0185858, 0.918882);"
			"	poisson[13] = vec2(0.0381787, -0.728996);"
			"	poisson[14] = vec2(0.16599, 0.093112);"
			"	poisson[15] = vec2(0.253639, 0.719535);"
			"	poisson[16] = vec2(0.369549, -0.655019);"
			"	poisson[17] = vec2(0.423627, 0.429975);"
			"	poisson[18] = vec2(0.530747, -0.364971);"
			"	poisson[19] = vec2(0.566027, -0.940489);"
			"	poisson[20] = vec2(0.639332, 0.0284127);"
			"	poisson[21] = vec2(0.652089, 0.669668);"
			"	poisson[22] = vec2(0.773797, 0.345012);"
			"	poisson[23] = vec2(0.968871, 0.840449);"
			"	poisson[24] = vec2(0.991882, -0.657338);"
			"	for (int i = 0; i < NUM_CASCADES; ++i) {"
			"	if (gl_FragCoord.z < cascadeEndClipSpace[i]) {"
			"		vec4 shadowCoord = lightSpacePos[i];\n" // shadowPos
			"#ifdef GL_EXT_gpu_shader4\n"
			"		vec2 dz_duv = depthGradient(shadowCoord.xy, shadowCoord.z);"
			"		shadowCoord.z -= min(2.0 * dot(vec2(1.0) / 1024.0, abs(dz_duv)), 0.005);"
			"		float sum = 0.0;"
			"		for (int j = 0; j < 25; ++j) {"
			"				vec2 offset = poisson[j] / 1024.0;"
			"				float shadowDepth = shadowCoord.z + dot(dz_duv, offset);"
			"				sum += shadow2D(shadowMap[i], vec3(shadowCoord.xy + offset, shadowDepth)).x;"
			"		}"
			"		return sum / 25.0;\n"
			"#else\n"
			"		return texture2D(shadowMap[i], shadowCoord.xy).x + 0.001 < shadowCoord.z ? 0.3 : 1.0;\n" // Slight offset to prevent shadow acne
			"#endif\n"
			"	}"
			"	}"
			"	return 1.0;"
			"}"
			"void main() {"
			"	float shadowFactor = calcShadowFactor();"
			"	float intensity = max(dot(normalize(vNormal), normalize(-lightDir)), 0.0);"
			"	gl_FragColor = vec4(shadowFactor * intensity * vec3(1.0, 1.0, 1.0), 1.0);"
			"}";
	GLuint program = renderer->program = createProgramVertFrag(vertexShaderSource, fragmentShaderSource);
	// Specify the layout of the vertex data
	renderer->posAttrib = glGetAttribLocation(program, "position");
	renderer->normalAttrib = glGetAttribLocation(program, "normal");
	// Get the location of program uniforms
	renderer->mvpUniform = glGetUniformLocation(program, "mvp");
	renderer->modelUniform = glGetUniformLocation(program, "model");
	glUseProgram(program);
	renderer->model = MatrixIdentity();
	renderer->projection = MatrixPerspective(FOV, (float) width / height, Z_NEAR, Z_FAR);
	glUniformMatrix4fv(renderer->modelUniform, 1, GL_FALSE, MatrixGet(mv, renderer->model));

	// Shadow mapping
	const GLchar *depthVertexShaderSource = "attribute vec3 position;"
		"uniform mat4 mvp;" // lightMVP
		"void main() {"
		"	gl_Position = mvp * vec4(position, 1.0);"
		"}",
		*depthFragmentShaderSource = "void main() {}";
	// renderer->depthProgram = createProgramVertFrag(depthVertexShaderSource, depthFragmentShaderSource);
	renderer->depthProgram = createProgram(1, createShader(GL_VERTEX_SHADER, 1, depthVertexShaderSource), 0);
	renderer->depthProgramPosition = glGetAttribLocation(renderer->depthProgram, "position");
	renderer->depthProgramMvp = glGetUniformLocation(renderer->depthProgram, "mvp");

	// Create the depth buffers
	glGenTextures(1, &renderer->depthTexture);
	glBindTexture(GL_TEXTURE_2D, renderer->depthTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glGenTextures(NUM_SPLITS, renderer->shadowMaps);
	for (int i = 0; i < NUM_SPLITS; ++i) {
		glBindTexture(GL_TEXTURE_2D, renderer->shadowMaps[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, DEPTH_SIZE, DEPTH_SIZE, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#ifdef GL_EXT_gpu_shader4
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
#endif
	}
	// Create the FBO
	glGenFramebuffers(1, &renderer->depthFbo);
	glBindFramebuffer(GL_FRAMEBUFFER, renderer->depthFbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, renderer->shadowMaps[0], 0);
	glDrawBuffer(GL_NONE); // Disable writes to the color buffer
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		printf("Error creating framebuffer.\n");
	}

	glGenTextures(1, &renderer->sceneTexture);
	glBindTexture(GL_TEXTURE_2D, renderer->sceneTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glGenFramebuffers(1, &renderer->sceneFbo);
	glBindFramebuffer(GL_FRAMEBUFFER, renderer->sceneFbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer->sceneTexture, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, renderer->depthTexture, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		printf("Error creating framebuffer.\n");
	}

	float quadVertices[] = { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f };
	glGenBuffers(1, &renderer->quadBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, renderer->quadBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof quadVertices, quadVertices, GL_STATIC_DRAW);

	const GLchar *fullscreenVertexShaderSource = "attribute vec2 position;"
		"varying vec2 texCoord;"
		"void main() {"
		"	gl_Position = vec4(position, 0.0, 1.0);"
		"	texCoord = 0.5 * vec2(position) + 0.5;"
		"}";
	GLuint fullscreenVertexShader = createShader(GL_VERTEX_SHADER, 1, fullscreenVertexShaderSource);

	// Screen Space Ambient Occlusion
	const GLchar *ssaoFragmentShaderSource = "#ifdef GL_ES\n"
			"precision mediump float;\n"
			"#endif\n"
			"#extension GL_EXT_gpu_shader4 : require\n"
			"#extension GL_OES_standard_derivatives : require\n"
			"#define NUM_SAMPLES (31)\n"
			"#define FAR_PLANE_Z (99.0)\n"
			"#define NUM_SPIRAL_TURNS (7)\n"
			"varying vec2 texCoord;"
			"uniform float radius;"
			"uniform float projScale;"
			"uniform sampler2D depthTexture;"
			"uniform float bias;"
			"uniform float intensityDivR6;"
			"uniform vec3 clipInfo;"
			"uniform vec4 projInfo;"
			"uniform vec2 invResolution;"
			"float reconstructCSZ(float d) {"
			"	return clipInfo[0] / (clipInfo[1] * d + clipInfo[2]);"
			"}"
			// Read the camera-space position of the point at screen-space position ssP.
			"vec3 getPosition(vec2 ssP) {"
			"	float z = reconstructCSZ(texture2D(depthTexture, ssP).r);"
			"	return vec3((ssP * projInfo.xy + projInfo.zw) * z, z);"
			"}"
			"float CSZToKey(float z) {"
			"	return clamp(z * (1.0 / FAR_PLANE_Z), 0.0, 1.0);"
			"}"
			"void packKey(float key, out vec2 p) {"
			"	float temp = floor(key * 256.0);" // Round to nearest 1 / 256.0
			"	p.x = temp * (1.0 / 256.0);" // Integer part
			"	p.y = key * 256.0 - temp;" // Fractional part
			"}"
			// Returns a unit vector and a screen-space radius for the tap on a unit disk (the caller should scale by the actual disk radius)
			"vec2 tapLocation(int sampleNumber, float spinAngle, out float ssR) {"
			"	ssR = (float(sampleNumber) + 0.5) * (1.0 / float(NUM_SAMPLES));"
			"	float angle = ssR * (float(NUM_SPIRAL_TURNS) * 6.28) + spinAngle;"
			"	return vec2(cos(angle), sin(angle));"
			"}"
			"float radius2 = radius * radius;"
			"float sampleAO(in vec2 ssC, in vec3 C, in vec3 n_C, in float ssDiskRadius, in int tapIndex, in float randomPatternRotationAngle) {"
			"	float ssR;"
			"	vec2 unitOffset = tapLocation(tapIndex, randomPatternRotationAngle, ssR);"
			"	vec3 Q = getPosition(ssC + ssR * ssDiskRadius * unitOffset * invResolution);"
			"	vec3 v = Q - C;"
			"	float vv = dot(v, v), vn = dot(v, n_C);"
			"	const float epsilon = 0.01;"
			"	float f = max(radius2 - vv, 0.0);"
			"	return f * f * f * max((vn - bias) / (epsilon + vv), 0.0);"
			"}"
			"void main() {"
			"	vec2 ssC = texCoord;" // Pixel being shaded
			"	vec3 C = getPosition(ssC);" // World space point being shaded
			"	if (C.z >= FAR_PLANE_Z) discard;"
			"	vec3 n_C = normalize(cross(dFdy(C), dFdx(C)));" // Reconstruct screen-space unit normal from screen-space position
			// "	float randomPatternRotationAngle = float((3 * ssC.x ^ ssC.y + ssC.x * ssC.y) * 10);"
			"	float randomPatternRotationAngle = 1000.0 * fract(52.9829189 * fract(dot(ssC, vec2(0.06711056, 0.00583715))));"
			"	float ssDiskRadius = projScale * radius / C.z;"
			"	float sum = 0.0;"
			"	for (int i = 0; i < NUM_SAMPLES; ++i) {"
			"		sum += sampleAO(ssC, C, n_C, ssDiskRadius, i, randomPatternRotationAngle);"
			"	}"
			"	float A = max(0.0, 1.0 - sum * intensityDivR6 * (5.0 / float(NUM_SAMPLES)));"
			// "	if (abs(dFdx(C.z)) < 0.02) A -= dFdx(A) * (float(ssC.x & 1) - 0.5);"
			// "	if (abs(dFdy(C.z)) < 0.02) A -= dFdy(A) * (float(ssC.y & 1) - 0.5);"
			"	gl_FragColor.r = A;"
			"	packKey(CSZToKey(C.z), gl_FragColor.gb);"
			"}";
	renderer->ssaoProgram = createProgram(2, fullscreenVertexShader, DONT_DELETE_SHADER,
			createShader(GL_FRAGMENT_SHADER, 1, ssaoFragmentShaderSource), 0);
	glUseProgram(renderer->ssaoProgram);
	const float radius = 1.0f;
	glUniform1f(glGetUniformLocation(renderer->ssaoProgram, "radius"), radius);
	glUniform1f(glGetUniformLocation(renderer->ssaoProgram, "bias"), 0.012f);
	glUniform1f(glGetUniformLocation(renderer->ssaoProgram, "intensityDivR6"), 1.0f / pow(radius, 6.0f));
	// Clipping plane constants for use by reconstructZ
	const float clipInfo[] = {Z_NEAR * Z_FAR, Z_NEAR - Z_FAR, Z_FAR};
	glUniform3fv(glGetUniformLocation(renderer->ssaoProgram, "clipInfo"), 1, clipInfo);
	const float projScale = width / (tanf(DEGREES_TO_RADIANS(FOV) * 0.5f) * 2.0f);
	glUniform1f(glGetUniformLocation(renderer->ssaoProgram, "projScale"), projScale);
	MatrixGet(mv, renderer->projection);
	const float projInfo[] = { 2.0f / mv[0], 2.0f / mv[5], -1.0f / mv[0], -1.0f / mv[5] };
	glUniform4fv(glGetUniformLocation(renderer->ssaoProgram, "projInfo"), 1, projInfo);
	glUniform2f(glGetUniformLocation(renderer->ssaoProgram, "invResolution"), 1.0f / width, 1.0f / height);

	glGenTextures(1, &renderer->ssaoTexture);
	glBindTexture(GL_TEXTURE_2D, renderer->ssaoTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glGenFramebuffers(1, &renderer->ssaoFbo);
	glBindFramebuffer(GL_FRAMEBUFFER, renderer->ssaoFbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer->ssaoTexture, 0);

	const GLchar *blurFragmentShaderSource = "varying vec2 texCoord;"
		"uniform sampler2D source;"
		"uniform vec2 invResolutionDirection;" // Either set x to 1/width or y to 1/height
		"uniform float sharpness;"
		"const float KERNEL_RADIUS = 3.0;"
		/* Returns a number on (0, 1) */
		"float unpackKey(vec2 p) {"
		"	return p.x * (256.0 / 257.0) + p.y * (1.0 / 257.0);"
		"}"
		"float blurFunction(vec2 uv, float r, float center_c, float center_d, inout float w_total) {"
		"	vec4 temp = texture2D(source, uv);"
		"	float c = temp.r;"
		"	float d = unpackKey(temp.gb);"
		"	const float blurSigma = float(KERNEL_RADIUS) * 0.5;"
		"	const float blurFalloff = 1.0 / (2.0 * blurSigma * blurSigma);"
		"	float ddiff = (d - center_d) * sharpness;"
		"	float w = exp2(-r * r * blurFalloff - ddiff * ddiff);"
		"	w_total += w;"
		"	return c * w;"
		"}"
		"void main() {"
		"	vec4 temp = texture2D(source, texCoord);"
		"	float center_c = temp.r;"
		"	float center_d = unpackKey(temp.gb);"

		"	float c_total = center_c;"
		"	float w_total = 1.0;"
		"	for (float r = 1.0; r <= KERNEL_RADIUS; ++r) {"
		"		vec2 uv = texCoord + invResolutionDirection * r;"
		"		c_total += blurFunction(uv, r, center_c, center_d, w_total);"
		"	}"
		"	for (float r = 1.0; r <= KERNEL_RADIUS; ++r) {"
		"		vec2 uv = texCoord - invResolutionDirection * r;"
		"		c_total += blurFunction(uv, r, center_c, center_d, w_total);"
		"	}\n"
		"#ifdef AO_PACK_KEY\n"
		"	gl_FragColor.r = c_total / w_total;"
		"	gl_FragColor.gb = temp.gb;\n"
		"#else\n"
		"	gl_FragColor = vec4(vec3(c_total / w_total), 1.0);\n"
		"#endif\n"
		"}";
	renderer->blur1Program = createProgram(2, fullscreenVertexShader, DONT_DELETE_SHADER,
			createShader(GL_FRAGMENT_SHADER, 2, "#define AO_PACK_KEY\n", blurFragmentShaderSource), 0);
	glUseProgram(renderer->blur1Program);
	const float sharpness = 40.0f;
	glUniform1f(glGetUniformLocation(renderer->blur1Program, "sharpness"), sharpness);
	renderer->blur2Program = createProgram(2, fullscreenVertexShader, DONT_DELETE_SHADER,
			createShader(GL_FRAGMENT_SHADER, 1, blurFragmentShaderSource), 0);
	glUseProgram(renderer->blur2Program);
	glUniform1f(glGetUniformLocation(renderer->blur2Program, "sharpness"), sharpness);

	glGenTextures(1, &renderer->blurTexture);
	glBindTexture(GL_TEXTURE_2D, renderer->blurTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glGenFramebuffers(1, &renderer->blurFbo);
	glBindFramebuffer(GL_FRAMEBUFFER, renderer->blurFbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer->blurTexture, 0);

	const GLchar *motionBlurFragmentShaderSource = "#define NUM_SAMPLES (10)\n"
		"varying vec2 texCoord;"
		"uniform sampler2D texture;"
		"uniform sampler2D depthTexture;"
		"uniform mat4 currentToPreviousMatrix;"
		"uniform float factor;"
		"void main() {"
		"	vec4 currentPos = vec4(2.0 * texCoord - 1.0, texture2D(depthTexture, texCoord).x, 1.0);"
		"	vec4 previousPos = currentToPreviousMatrix * currentPos;"
		"	previousPos /= previousPos.w;"
		"	vec2 velocity = factor * (currentPos.xy - previousPos.xy) * 0.5;"
		"	vec4 result = texture2D(texture, texCoord);"
		"	for (int i = 1; i < NUM_SAMPLES; ++i) {"
		"		vec2 offset = velocity * (float(i) / float(NUM_SAMPLES - 1) - 0.5);"
		"		result += texture2D(texture, texCoord + offset);"
		"	}"
		"	gl_FragColor = result / float(NUM_SAMPLES);"
		"}";
	renderer->motionBlurProgram = createProgram(2, fullscreenVertexShader, 0,
			createShader(GL_FRAGMENT_SHADER, 1, motionBlurFragmentShaderSource), 0);
	glUseProgram(renderer->motionBlurProgram);
	glUniform1i(glGetUniformLocation(renderer->motionBlurProgram, "depthTexture"), 1);
	renderer->motionBlurCurrToPrevUniform = glGetUniformLocation(renderer->motionBlurProgram, "currentToPreviousMatrix");
	renderer->motionBlurFactorUniform = glGetUniformLocation(renderer->motionBlurProgram, "factor");

	// Skybox
	char *skyboxData = readFile("skybox.dds");
	renderer->skyboxTexture = dds_load_texture_from_memory(skyboxData, 0, 0, 0);
	free(skyboxData);
	if (!renderer->skyboxTexture) {
		printf("Failed to load skybox texture.\n");
	}
	glBindTexture(GL_TEXTURE_CUBE_MAP, renderer->skyboxTexture);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	const GLchar *skyboxVertexShaderSource = "attribute vec2 position;"
		"uniform mat4 invProjection;"
		"uniform mat4 modelView;"
		"varying vec3 eyeDirection;"
		"void main() {"
		"	eyeDirection = vec3(invProjection * (gl_Position = vec4(position, 0.0, 1.0)) * modelView);"
		"}",
		*skyboxFragmentShaderSource = "#ifdef GL_ES\n"
			"precision mediump float;\n"
			"#endif\n"
			"varying vec3 eyeDirection;"
			"uniform samplerCube texture;"
			"void main() {"
			"	gl_FragColor = textureCube(texture, eyeDirection);"
			"}";
	renderer->skyboxProgram = createProgramVertFrag(skyboxVertexShaderSource, skyboxFragmentShaderSource);
	renderer->skyboxPositionAttrib = glGetAttribLocation(renderer->skyboxProgram, "position");

	glBindFramebuffer(GL_FRAMEBUFFER, 0); // TODO remove
}

void rendererDestroy(struct Renderer *renderer) {
	glDeleteProgram(renderer->program);
	glDeleteFramebuffers(1, &renderer->sceneFbo);
	glDeleteTextures(1, &renderer->sceneTexture);
	glDeleteProgram(renderer->depthProgram);
	glDeleteFramebuffers(1, &renderer->depthFbo);
	glDeleteTextures(1, &renderer->depthTexture);
	glDeleteTextures(NUM_SPLITS, renderer->shadowMaps);

	glDeleteBuffers(1, &renderer->quadBuffer);
	glDeleteProgram(renderer->ssaoProgram);
	glDeleteTextures(1, &renderer->ssaoTexture);
	glDeleteFramebuffers(1, &renderer->ssaoFbo);
	glDeleteProgram(renderer->blur1Program);
	glDeleteProgram(renderer->blur2Program);
	glDeleteTextures(1, &renderer->blurTexture);
	glDeleteFramebuffers(1, &renderer->blurFbo);
	glDeleteProgram(renderer->motionBlurProgram);
	glDeleteTextures(1, &renderer->skyboxTexture);
	glDeleteProgram(renderer->skyboxProgram);
}

void rendererDraw(struct Renderer *renderer, VECTOR position, float yaw, float pitch, float roll, float dt) {
	ALIGN(16) float vv[4], mv[16];
	struct EntityManager *manager = renderer->manager;
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	const MATRIX bias = MatrixSet(
			0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.5f, 0.0f,
			0.5f, 0.5f, 0.5f, 1.0f);
	const VECTOR viewDir = VectorSet(-cos(pitch) * sin(yaw), sin(pitch), -cos(pitch) * cos(yaw), 0.0f);

	const MATRIX rotationViewMatrix = MatrixRotationQuaternion(QuaternionRotationRollPitchYaw(pitch, yaw, roll)),
		  viewInverse = MatrixMultiply(MatrixTranslationFromVector(position), rotationViewMatrix);
	renderer->view = MatrixInverse(viewInverse);
	const MATRIX modelView = MatrixMultiply(renderer->view, renderer->model),
		  invModelView = MatrixInverse(modelView);
	MATRIX mvp = MatrixMultiply(renderer->projection, MatrixMultiply(renderer->view, renderer->model));
	VECTOR up = VectorTransform(VectorSet(0.0f, 1.0f, 0.0f, 0.0f), rotationViewMatrix);

	// Shadow map pass
	glViewport(0, 0, DEPTH_SIZE, DEPTH_SIZE);
	glUseProgram(renderer->depthProgram);
	glEnableVertexAttribArray(renderer->depthProgramPosition);
	glBindFramebuffer(GL_FRAMEBUFFER, renderer->depthFbo);
	// glCullFace(GL_FRONT); // Avoid peter-panning
	const VECTOR lightDir = Vector4Normalize(VectorSet(-1.0f, -1.0f, 1.0f, 0.0f));
	const MATRIX lightView = lookAt(VectorSet(0.0f, 0.0f, 0.0f, 1.0f), lightDir, VectorSet(0.0f, 1.0f, 0.0f, 0.0f));
	float splitDistances[NUM_SPLITS + 1];
	getSplitDistances(splitDistances, Z_NEAR, Z_FAR);
	struct Frustum f[NUM_SPLITS];
	MATRIX shadowCPM[NUM_SPLITS];
	for (int i = 0; i < NUM_SPLITS; ++i) {
		f[i].fov = DEGREES_TO_RADIANS(FOV) + 0.2f;
		f[i].ratio = (float) renderer->width / renderer->height;
		f[i].neard = splitDistances[i];
		f[i].fard = splitDistances[i + 1] * 1.005f;

		// Bind and clear current cascade
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, renderer->shadowMaps[i], 0);
		glClear(GL_DEPTH_BUFFER_BIT);

		// Compute camera frustum slice boundary points in world space
		VECTOR frustumPoints[8];
		getFrustumPoints(f[i], position, viewDir, up, frustumPoints);
		calculateCropMatrix(f[i], frustumPoints, lightView, shadowCPM + i);
		glUniformMatrix4fv(renderer->depthProgramMvp, 1, GL_FALSE, MatrixGet(mv, shadowCPM[i]));

		// Draw the model
		for (int j = 0; j < MAX_ENTITIES; ++j) {
			if ((manager->entityMasks[j] & RENDER_MASK) == RENDER_MASK) {
				drawModelGeometry(manager->models[j].model, renderer->depthProgramPosition);
			}
		}
	}
	// glCullFace(GL_BACK);
	glViewport(0, 0, renderer->width, renderer->height);

	// Depth pass
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, renderer->depthTexture, 0);
	glClear(GL_DEPTH_BUFFER_BIT);
	glUniformMatrix4fv(renderer->depthProgramMvp, 1, GL_FALSE, MatrixGet(mv, mvp));
	for (int j = 0; j < MAX_ENTITIES; ++j) {
		if ((manager->entityMasks[j] & RENDER_MASK) == RENDER_MASK) drawModelGeometry(manager->models[j].model, renderer->depthProgramPosition);
	}
	glDisableVertexAttribArray(renderer->depthProgramPosition);

	// Main pass: render scene as normal with shadow mapping (using depth map)
	glBindFramebuffer(GL_FRAMEBUFFER, renderer->sceneFbo);
	glClear(GL_COLOR_BUFFER_BIT); // Clear the screen

	// Draw the skybox
	glDisable(GL_DEPTH_TEST);
	glUseProgram(renderer->skyboxProgram);
	glUniformMatrix4fv(glGetUniformLocation(renderer->skyboxProgram, "invProjection"), 1, GL_FALSE, MatrixGet(mv, MatrixInverse(renderer->projection)));
	glUniformMatrix4fv(glGetUniformLocation(renderer->skyboxProgram, "modelView"), 1, GL_FALSE, MatrixGet(mv, modelView));
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, renderer->skyboxTexture);
	glBindBuffer(GL_ARRAY_BUFFER, renderer->quadBuffer);
	glEnableVertexAttribArray(renderer->skyboxPositionAttrib);
	glVertexAttribPointer(renderer->skyboxPositionAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(renderer->skyboxPositionAttrib);
	glEnable(GL_DEPTH_TEST);

	// Draw the scene
	glDepthFunc(GL_EQUAL);
	glDepthMask(GL_FALSE);
	glUseProgram(renderer->program);
	glUniformMatrix4fv(renderer->mvpUniform, 1, GL_FALSE, MatrixGet(mv, mvp));
	float cascadeEndClipSpace[3];
	GLfloat shadowCPMValues[NUM_SPLITS * 16];
	GLint depthTextures[NUM_SPLITS];
	MatrixGet(mv, renderer->projection);
	for (int i = 0; i < NUM_SPLITS; ++i) {
		// Compute split far distance in camera homogeneous coordinates and normalize to [0, 1]
		const float farBound = 0.5f * (-f[i].fard * mv[10] + mv[14]) / f[i].fard + 0.5f;
		cascadeEndClipSpace[i] = farBound;

		MatrixGet(shadowCPMValues + 16 * i, MatrixMultiply(bias, shadowCPM[i]));

		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, renderer->shadowMaps[i]);
		depthTextures[i] = i;
	}
	glUniform1fv(glGetUniformLocation(renderer->program, "cascadeEndClipSpace"), NUM_SPLITS, cascadeEndClipSpace);
	glUniformMatrix4fv(glGetUniformLocation(renderer->program, "lightMVP"), NUM_SPLITS, GL_FALSE, shadowCPMValues);
	glUniform1iv(glGetUniformLocation(renderer->program, "shadowMap"), NUM_SPLITS, depthTextures);
	glUniform3fv(glGetUniformLocation(renderer->program, "lightDir"), 1, VectorGet(vv, lightDir));

	glEnableVertexAttribArray(renderer->posAttrib);
	glEnableVertexAttribArray(renderer->normalAttrib);
	for (int j = 0; j < MAX_ENTITIES; ++j) {
		if ((manager->entityMasks[j] & RENDER_MASK) == RENDER_MASK) drawModelGeometryNormal(manager->models[j].model, renderer->posAttrib, renderer->normalAttrib);
	}
	glDisableVertexAttribArray(renderer->posAttrib);
	glDisableVertexAttribArray(renderer->normalAttrib);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);

	glDisable(GL_DEPTH_TEST);
	glBindBuffer(GL_ARRAY_BUFFER, renderer->quadBuffer);

	// Draw ambient occlusion
	glBindFramebuffer(GL_FRAMEBUFFER, renderer->ssaoFbo);
	glClear(GL_COLOR_BUFFER_BIT);
	glUseProgram(renderer->ssaoProgram);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, renderer->depthTexture);
	glEnableVertexAttribArray(glGetAttribLocation(renderer->ssaoProgram, "position"));
	glVertexAttribPointer(glGetAttribLocation(renderer->ssaoProgram, "position"), 2, GL_FLOAT, GL_FALSE, 0, 0);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(glGetAttribLocation(renderer->ssaoProgram, "position"));

	glBindFramebuffer(GL_FRAMEBUFFER, renderer->blurFbo);
	glClear(GL_COLOR_BUFFER_BIT);
	glUseProgram(renderer->blur1Program);
	glBindTexture(GL_TEXTURE_2D, renderer->ssaoTexture);
	glEnableVertexAttribArray(glGetAttribLocation(renderer->blur1Program, "position"));
	glVertexAttribPointer(glGetAttribLocation(renderer->blur1Program, "position"), 2, GL_FLOAT, GL_FALSE, 0, 0);
	glUniform2f(glGetUniformLocation(renderer->blur1Program, "invResolutionDirection"), 1.0f / renderer->width, 0);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(glGetAttribLocation(renderer->blur1Program, "position"));

	glUseProgram(renderer->blur2Program);
	glBindFramebuffer(GL_FRAMEBUFFER, renderer->sceneFbo);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ZERO, GL_SRC_COLOR);
	glBindTexture(GL_TEXTURE_2D, renderer->blurTexture);
	glUniform2f(glGetUniformLocation(renderer->blur2Program, "invResolutionDirection"), 0, 1.0f / renderer->height);
	glEnableVertexAttribArray(glGetAttribLocation(renderer->blur2Program, "position"));
	glVertexAttribPointer(glGetAttribLocation(renderer->blur2Program, "position"), 2, GL_FLOAT, GL_FALSE, 0, 0);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(glGetAttribLocation(renderer->blur2Program, "position"));
	glDisable(GL_BLEND);

	// Draw motion blur
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	glUseProgram(renderer->motionBlurProgram);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, renderer->sceneTexture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, renderer->depthTexture);
	MATRIX viewProjectionInverse = MatrixInverse(mvp);
	glUniformMatrix4fv(renderer->motionBlurCurrToPrevUniform, 1, GL_FALSE, MatrixGet(mv, MatrixMultiply(renderer->prevViewProjection, viewProjectionInverse)));
	glUniform1f(renderer->motionBlurFactorUniform, 20.0f / dt);
	renderer->prevViewProjection = MatrixMultiply(renderer->projection, renderer->view);
	glEnableVertexAttribArray(glGetAttribLocation(renderer->motionBlurProgram, "position"));
	glVertexAttribPointer(glGetAttribLocation(renderer->motionBlurProgram, "position"), 2, GL_FLOAT, GL_FALSE, 0, 0);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(glGetAttribLocation(renderer->motionBlurProgram, "position"));
}
