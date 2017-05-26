#include "gameState.h"
#include <stdio.h>
#include <math.h>
#include <SDL.h>
#include "pngloader.h"
#include "glUtil.h"
#include "ddsloader.h"
#include <float.h>

#include "image.h"
#include "label.h"

#define MOUSE_SENSITIVITY 0.006f
#define MOVEMENT_SPEED .02f

#define DEGREES_TO_RADIANS(a) ((a) * M_PI / 180)
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define NUM_FRUSTUM_CORNERS 8

#define DEPTH_SIZE 1024
#define zNear 1.0f
#define zFar 100.0f

struct Frustum {
	float neard;
	float fard;
	// In radians
	float fov;
	float ratio;
};

static void printVector(VECTOR v) {
	ALIGN(16) float vv[4];
	VectorGet(vv, v);
	printf("vector, %f, %f, %f, %f\n", vv[0], vv[1], vv[2], vv[3]);
}

static void printMatrix(MATRIX m) {
	ALIGN(16) float mv[16];
	MatrixGet(mv, m);
	printf("%f\t%f\t%f\t%f\n%f\t%f\t%f\t%f\n%f\t%f\t%f\t%f\n%f\t%f\t%f\t%f\n",
			mv[0], mv[1], mv[2], mv[3],
			mv[4], mv[5], mv[6], mv[7],
			mv[8], mv[9], mv[10], mv[11],
			mv[12], mv[13], mv[14], mv[15]);
}

static void gameStateUpdate(struct State *state, float dt) {
	struct GameState *gameState = (struct GameState *) state;
	int x, y;
	Uint32 button = SDL_GetRelativeMouseState(&x, &y);
	gameState->yaw -= x * MOUSE_SENSITIVITY;
	gameState->pitch -= y * MOUSE_SENSITIVITY;
	if (gameState->yaw > M_PI) gameState->yaw -= 2 * M_PI;
	else if (gameState->yaw < -M_PI) gameState->yaw += 2 * M_PI;
	// Clamp the pitch
	gameState->pitch = gameState->pitch < -M_PI / 2 ? -M_PI / 2 : gameState->pitch > M_PI / 2 ? M_PI / 2 : gameState->pitch;
	VECTOR forward = VectorSet(-MOVEMENT_SPEED * sin(gameState->yaw) * dt, 0, -MOVEMENT_SPEED * cos(gameState->yaw) * dt, 0),
		   up = VectorSet(0, 1, 0, 0),
		   right = VectorCross(forward, up);
	const Uint8 *keys = SDL_GetKeyboardState(NULL);
	VECTOR position = gameState->position;
	if (keys[SDL_SCANCODE_W]) position = VectorAdd(position, forward);
	if (keys[SDL_SCANCODE_A]) position = VectorSubtract(position, right);
	if (keys[SDL_SCANCODE_S]) position = VectorSubtract(position, forward);
	if (keys[SDL_SCANCODE_D]) position = VectorAdd(position, right);
	if (keys[SDL_SCANCODE_SPACE]) position = VectorAdd(position, VectorSet(0, MOVEMENT_SPEED * dt, 0, 0));
	if (keys[SDL_SCANCODE_LSHIFT]) position = VectorSubtract(position, VectorSet(0, MOVEMENT_SPEED * dt, 0, 0));
	gameState->position = position;
}

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
		// f[i - 1].fard = f[i].neard * 1.005f;
	}
	splitDistances[NUM_SPLITS] = far;
}

/**
 * Computes the 8 corners of the current view frustum.
 * @param points Gets set to the corners of the frustum.
 */
static void getFrustumPoints(struct Frustum f, VECTOR center, VECTOR viewDir, VECTOR *points) {
	VECTOR up = VectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	VECTOR right = VectorCross(viewDir, up);
	right = Vector4Normalize(right);
	up = Vector4Normalize(VectorCross(right, viewDir));
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

static void gameStateDraw(struct State *state, float dt) {
	struct GameState *gameState = (struct GameState *) state;
	struct SpriteBatch *batch = gameState->batch;
	ALIGN(16) float vv[4], mv[16];
	glEnable(GL_DEPTH_TEST);

	const VECTOR viewDir = VectorSet(-cos(gameState->pitch) * sin(gameState->yaw), sin(gameState->pitch), -cos(gameState->pitch) * cos(gameState->yaw), 0.0f);

	const MATRIX rotationViewMatrix = MatrixRotationQuaternion(QuaternionRotationRollPitchYaw(gameState->pitch, gameState->yaw, 0)),
		  viewInverse = MatrixMultiply(
				  MatrixTranslationFromVector(gameState->position),
				  rotationViewMatrix);
	gameState->view = MatrixInverse(viewInverse);
	const MATRIX modelView = MatrixMultiply(gameState->view, gameState->model),
		  invModelView = MatrixInverse(modelView);

	// Shadow map pass
	glViewport(0, 0, DEPTH_SIZE, DEPTH_SIZE);
	glUseProgram(gameState->depthProgram);
	glActiveTexture(GL_TEXTURE0);
	// Offset the geometry slightly to prevent Z-fighting
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(1.0f, 4096.0f);
	// glCullFace(GL_FRONT); // Avoid peter-panning
	glDisable(GL_CULL_FACE);

	float splitDistances[NUM_SPLITS + 1];
	getSplitDistances(splitDistances, 1.0f, 100.0f);
	struct Frustum f[NUM_SPLITS];
	for (int i = 0; i < NUM_SPLITS; ++i) {
		f[i].fov = DEGREES_TO_RADIANS(90.0f) + 0.2f;
		f[i].ratio = (float) 800 / 600;
		f[i].neard = splitDistances[i];
		f[i].fard = splitDistances[i + 1];
	}
	const VECTOR lightDir = VectorSet(1.0f, -1.0f, 1.0f, 0.0f);
	const MATRIX lightView = lookAt(VectorSet(0.0f, 0.0f, 0.0f, 1.0f), lightDir, VectorSet(0.0f, 1.0f, 0.0f, 0.0f));

	MATRIX shadowCPM[NUM_SPLITS];
	for (int i = 0; i < NUM_SPLITS; ++i) {
		// Bind and clear current cascade
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gameState->depthFbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gameState->shadowMaps[i], 0);
		glClear(GL_DEPTH_BUFFER_BIT);

		// Compute camera frustum slice boundary points in world space
		VECTOR frustumPoints[8];
		getFrustumPoints(f[i], gameState->position, viewDir, frustumPoints);
		MATRIX shadowMatrix;
		calculateCropMatrix(f[i], frustumPoints, lightView, shadowCPM + i);
		glUniformMatrix4fv(glGetUniformLocation(gameState->depthProgram, "lightMVP"), 1, GL_FALSE, MatrixGet(mv, shadowCPM[i]));

		// Draw the model
		const GLuint attrib = glGetAttribLocation(gameState->depthProgram, "position");
		glEnableVertexAttribArray(attrib);

		glBindBuffer(GL_ARRAY_BUFFER, gameState->objModel->vertexBuffer);
		glVertexAttribPointer(attrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gameState->objModel->indexBuffer);
		glDrawElements(GL_TRIANGLES, gameState->objModel->indexCount, GL_UNSIGNED_INT, 0);

		glBindBuffer(GL_ARRAY_BUFFER, gameState->groundModel->vertexBuffer);
		glVertexAttribPointer(attrib, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gameState->groundModel->indexBuffer);
		glDrawElements(GL_TRIANGLES, gameState->groundModel->indexCount, GL_UNSIGNED_INT, 0);

		glDisableVertexAttribArray(attrib);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
	glDisable(GL_POLYGON_OFFSET_FILL);

	// Main pass: render scene as normal with shadow mapping (using depth map)
	// glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);
	glViewport(0, 0, 800, 600);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear the screen

	// Draw the model
	glUseProgram(gameState->program);
	glUniformMatrix4fv(gameState->viewUniform, 1, GL_FALSE, MatrixGet(mv, gameState->view));
	glEnableVertexAttribArray(gameState->posAttrib);

	const MATRIX bias = MatrixSet(
			0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.5f, 0.0f,
			0.5f, 0.5f, 0.5f, 1.0f);
	float cascadeEndClipSpace[3];
	GLfloat shadowCPMValues[NUM_SPLITS * 16];
	GLint depthTextures[NUM_SPLITS];
	for (int i = 0; i < NUM_SPLITS; ++i) {
		// Compute split far distance in camera homogeneous coordinates and normalize to [0, 1]
		MatrixGet(mv, gameState->projection);
		const float farBound = 0.5f * (-f[i].fard * mv[10] + mv[14]) / f[i].fard + 0.5f;
		cascadeEndClipSpace[i] = farBound;

		MatrixGet(shadowCPMValues + 16 * i, MatrixMultiply(bias, shadowCPM[i]));

		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, gameState->shadowMaps[i]);
		depthTextures[i] = i;
	}
	glUniform1fv(glGetUniformLocation(gameState->program, "cascadeEndClipSpace"), NUM_SPLITS, cascadeEndClipSpace);
	glUniformMatrix4fv(glGetUniformLocation(gameState->program, "lightMVP"), NUM_SPLITS, GL_FALSE, shadowCPMValues);
	glUniform1iv(glGetUniformLocation(gameState->program, "shadowMap"), NUM_SPLITS, depthTextures);

	struct Model *model = gameState->objModel;
	glBindBuffer(GL_ARRAY_BUFFER, model->vertexBuffer);
	glVertexAttribPointer(gameState->posAttrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model->indexBuffer);
	glDrawElements(GL_TRIANGLES, model->indexCount, GL_UNSIGNED_INT, 0);

	struct Model *groundModel = gameState->groundModel;
	glBindBuffer(GL_ARRAY_BUFFER, groundModel->vertexBuffer);
	glVertexAttribPointer(gameState->posAttrib, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, groundModel->indexBuffer);
	glDrawElements(GL_TRIANGLES, groundModel->indexCount, GL_UNSIGNED_INT, 0);

	glDisableVertexAttribArray(gameState->posAttrib);

	// Draw the skybox
	glUseProgram(gameState->skyboxProgram);
	glUniformMatrix4fv(glGetUniformLocation(gameState->skyboxProgram, "invProjection"), 1, GL_FALSE, MatrixGet(mv, MatrixInverse(gameState->projection)));
	glUniformMatrix4fv(glGetUniformLocation(gameState->skyboxProgram, "modelView"), 1, GL_FALSE, MatrixGet(mv, modelView));
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, gameState->skyboxTexture);
	glBindBuffer(GL_ARRAY_BUFFER, gameState->skyboxVertexBuffer);
	glEnableVertexAttribArray(glGetAttribLocation(gameState->skyboxProgram, "vertex"));
	glVertexAttribPointer(glGetAttribLocation(gameState->skyboxProgram, "vertex"), 2, GL_FLOAT, GL_FALSE, 0, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gameState->skyboxIndexBuffer);
	glDepthFunc(GL_LEQUAL);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	glDepthFunc(GL_LESS);

	// Draw GUI
	glDisable(GL_DEPTH_TEST);
	spriteBatchBegin(batch);
	widgetValidate(gameState->flexLayout, 800, 600);
	widgetDraw(gameState->flexLayout, batch);
	spriteBatchEnd(batch);
}

static void gameStateResize(struct State *state, int width, int height) {
	struct GameState *gameState = (struct GameState *) state;
	widgetLayout(gameState->flexLayout, width, MEASURE_EXACTLY, height, MEASURE_EXACTLY);

	gameState->projection = MatrixPerspective(90.f, (float) width / height, 1.0f, 100.0f);
	glUseProgram(gameState->program);
	ALIGN(16) float mv[16];
	glUniformMatrix4fv(gameState->projectionUniform, 1, GL_FALSE, MatrixGet(mv, gameState->projection));
}

static struct FlexParams params0 = { ALIGN_END, -1, 100, UNDEFINED, 20, 0, 20, 20 },
						 params2 = {ALIGN_CENTER, 1, 100, UNDEFINED, 0, 0, 0, 50},
						 params1 = { ALIGN_CENTER, 1, UNDEFINED, UNDEFINED, 0, 0, 0, 0 };

void gameStateInitialize(struct GameState *gameState, struct SpriteBatch *batch) {
	struct State *state = (struct State *) gameState;
	state->update = gameStateUpdate;
	state->draw = gameStateDraw;
	state->resize = gameStateResize;
	gameState->batch = batch;

	const GLchar *vertexShaderSource = "#version 150 core\n"
		"in vec3 position;"
		"uniform mat4 model;"
		"uniform mat4 view;"
		"uniform mat4 projection;"
		"const int NUM_CASCADES = 3;"
		"uniform mat4 lightMVP[NUM_CASCADES];"
		"varying vec4 lightSpacePos[NUM_CASCADES];"
		"void main() {"
		"	gl_Position = projection * view * model * vec4(position, 1.0);"
		"	for (int i = 0; i < NUM_CASCADES; ++i) {"
		"		lightSpacePos[i] = lightMVP[i] * vec4(position, 1.0);"
		"	}"
		"}",
		*fragmentShaderSource = "#version 150 core\n"
			"const int NUM_CASCADES = 3;"
			"varying vec4 lightSpacePos[NUM_CASCADES];"
			"uniform sampler2D shadowMap[NUM_CASCADES];"
			"uniform float cascadeEndClipSpace[NUM_CASCADES];"
			"uniform mat4 lightMVP[NUM_CASCADES];"
			"uniform vec4 color[4] = vec4[4](vec4(1.0, 0.0, 0.0, 1.0),"
			"	vec4(0.0, 1.0, 0.0, 1.0),"
			"	vec4(0.0, 0.0, 1.0, 1.0),"
			"	vec4(1.0, 1.0, 1.0, 1.0));"
			"out vec4 outColor;"
			"float calcShadowFactor(int cascadeIndex, vec4 shadowCoord) {"
			// "	shadowCoord = vec4(0.5 * shadowCoord.xyz + 0.5, shadowCoord.w);"
			"	float z = shadowCoord.z;"
			"	float depth = texture(shadowMap[cascadeIndex], shadowCoord.xy).x;"
			"	return depth < z ? 0.3 : 1.0;"
			"}"
			"void main() {"
			"	float shadowFactor = 0.0;"
			"	int i;"
			"	for (i = 0; i < NUM_CASCADES; ++i) {"
			"		if (gl_FragCoord.z < cascadeEndClipSpace[i]) {"
			"			shadowFactor = calcShadowFactor(i, lightSpacePos[i]);"
			"			break;"
			"		}"
			"	}"
			"	outColor = vec4(shadowFactor * vec3(1.0, 1.0, 1.0), 1.0) * color[i];"
			"}";
	GLuint program = createProgram(vertexShaderSource, fragmentShaderSource);
	glBindFragDataLocation(program, 0, "outColor");
	glLinkProgram(program);
	gameState->program = program;

	// Specify the layout of the vertex data
	GLint posAttrib = glGetAttribLocation(program, "position");
	// Get the location of program uniforms
	GLint modelUniform = glGetUniformLocation(program, "model");
	GLint viewUniform = glGetUniformLocation(program, "view");
	GLint projectionUniform = glGetUniformLocation(program, "projection");
	gameState->posAttrib = posAttrib;
	gameState->modelUniform = modelUniform;
	gameState->viewUniform = viewUniform;
	gameState->projectionUniform = projectionUniform;
	ALIGN(16) float vv[4], mv[16];
	glUseProgram(program);
	gameState->model = MatrixIdentity();
	gameState->projection = MatrixPerspective(90.f, 800.0f / 600.0f, 1.0f, 100.0f);
	glUniformMatrix4fv(modelUniform, 1, GL_FALSE, MatrixGet(mv, gameState->model));
	glUniformMatrix4fv(projectionUniform, 1, GL_FALSE, MatrixGet(mv, gameState->projection));

	// Skybox
	char *skyboxData = readFile("skybox.dds");
	gameState->skyboxTexture = dds_load_texture_from_memory(skyboxData, 0, 0, 0);
	free(skyboxData);
	if (!gameState->skyboxTexture) {
		printf("Failed to load skybox texture.\n");
	}
	glBindTexture(GL_TEXTURE_CUBE_MAP, gameState->skyboxTexture);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	const GLchar *skyboxVertexShaderSource = "attribute vec2 vertex;"
		"uniform mat4 invProjection;"
		"uniform mat4 modelView;"
		"varying vec3 eyeDirection;"
		"void main() {"
		"	eyeDirection = vec3(invProjection * (gl_Position = vec4(vertex, 0.0, 1.0)) * modelView);"
		"}",
		*skyboxFragmentShaderSource = "varying vec3 eyeDirection;"
			"uniform samplerCube texture;"
			"void main() {"
			"	gl_FragColor = textureCube(texture, eyeDirection);"
			"	gl_FragDepth = 1.0;"
			"}";
	gameState->skyboxProgram = createProgram(skyboxVertexShaderSource, skyboxFragmentShaderSource);
	glLinkProgram(gameState->skyboxProgram);
	const float skyboxVertices[] = { -1, -1, 1, -1, 1, 1, -1, 1 };
	const unsigned int cubeIndices[] = { 0, 1, 2, 0, 2, 3 };
	GLuint skyboxVertexBuffer, skyboxIndexBuffer;
	glGenBuffers(1, &skyboxVertexBuffer);
	glGenBuffers(1, &skyboxIndexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, skyboxVertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(GLfloat), skyboxVertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, skyboxIndexBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * sizeof(unsigned int), cubeIndices, GL_STATIC_DRAW);
	gameState->skyboxVertexBuffer = skyboxVertexBuffer;
	gameState->skyboxIndexBuffer = skyboxIndexBuffer;

	// Shadow mapping
	const GLchar *depthVertexShaderSource = "attribute vec3 position;"
		"uniform mat4 lightMVP;"
		"uniform mat4 model;"
		"void main() {"
		// "	gl_Position = lightMVP * model * vec4(position, 1.0f);"
		"	gl_Position = lightMVP * vec4(position, 1.0f);"
		"}",
		*depthFragmentShaderSource = "void main() {}";
	gameState->depthProgram = createProgram(depthVertexShaderSource, depthFragmentShaderSource);
	glLinkProgram(gameState->depthProgram);

	// Create the depth buffers
	glGenTextures(NUM_SPLITS, gameState->shadowMaps);
	for (int i = 0; i < NUM_SPLITS; ++i) {
		glBindTexture(GL_TEXTURE_2D, gameState->shadowMaps[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, DEPTH_SIZE, DEPTH_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		/*glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
		  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);*/
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
	}

	// Create the FBO
	GLuint depthFbo;
	glGenFramebuffers(1, &depthFbo);
	gameState->depthFbo = depthFbo;
	glBindFramebuffer(GL_FRAMEBUFFER, depthFbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gameState->shadowMaps[0], 0);
	glDrawBuffer(GL_NONE); // Disable writes to the color buffer
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		printf("Error creating framebuffer.\n");
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0); // TODO remove

	gameState->position = VectorSet(0, 0, 0, 1);
	gameState->yaw = 0;
	gameState->pitch = 0;
	gameState->objModel = loadModelFromObj("cube.obj");
	if (!gameState->objModel) {
		printf("Failed to load model.\n");
	}
	gameState->groundModel = loadModelFromObj("ground.obj");
	if (!gameState->groundModel) {
		printf("Failed to load ground model.\n");
	}

	// Initialize GUI
	gameState->flexLayout = malloc(sizeof(struct FlexLayout));
	flexLayoutInitialize(gameState->flexLayout, DIRECTION_ROW, ALIGN_START);

	png_uint_32 width, height;
	gameState->cat = loadPNGTexture("cat.png", &width, &height);
	if (!gameState->cat) {
		fprintf(stderr, "Failed to load png image.\n");
	}
	gameState->image0 = malloc(sizeof(struct Image));
	imageInitialize(gameState->image0, gameState->cat, width, height, 0);
	gameState->image0->layoutParams = &params0;
	containerAddChild(gameState->flexLayout, gameState->image0);

	gameState->image1 = malloc(sizeof(struct Image));
	imageInitialize(gameState->image1, gameState->cat, width, height, 0);
	gameState->image1->layoutParams = &params1;
	containerAddChild(gameState->flexLayout, gameState->image1);

	gameState->font = loadFont("DejaVuSans.ttf", 512, 512);
	if (!gameState->font) {
		printf("Could not load font.");
	}

	gameState->label = labelNew(gameState->font, "Axel ffi! and the AV. HHHHHHHH Hi! (215): tv-hund. fesflhslg");
	gameState->label->layoutParams = &params2;
	containerAddChild(gameState->flexLayout, gameState->label);
}

void gameStateDestroy(struct GameState *gameState) {
	glDeleteProgram(gameState->program);
	glDeleteProgram(gameState->skyboxProgram);
	glDeleteTextures(1, &gameState->skyboxTexture);
	glDeleteBuffers(1, &gameState->skyboxVertexBuffer);
	glDeleteBuffers(1, &gameState->skyboxIndexBuffer);

	destroyModel(gameState->objModel);

	fontDestroy(gameState->font);
	containerDestroy(gameState->flexLayout);
	free(gameState->flexLayout);
	free(gameState->image0);
	free(gameState->image1);
	labelDestroy(gameState->label);
	free(gameState->label);
	glDeleteTextures(1, &gameState->cat);
}
