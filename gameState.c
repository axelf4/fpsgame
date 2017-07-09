#include "gameState.h"
#include <stdio.h>
#include <math.h>
#include <SDL.h>
#include "pngloader.h"
#include "glUtil.h"
#include "ddsloader.h"
#include <float.h>
#include <stdint.h>

#include "image.h"
#include "label.h"

#define MOUSE_SENSITIVITY 0.006f
#define MOVEMENT_SPEED .02f

#define DEGREES_TO_RADIANS(a) ((a) * M_PI / 180)
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define NUM_FRUSTUM_CORNERS 8

#define DEPTH_SIZE 1024
// TODO rename these
#define zNear (1.0f)
#define zFar (100.0f)
#define GUARD_BAND_SIZE (800 / 10)

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
	const VECTOR lightDir = VectorSet(1.0f, -1.0f, 0.5f, 0.0f);
	const MATRIX lightView = lookAt(VectorSet(0.0f, 0.0f, 0.0f, 1.0f), lightDir, VectorSet(0.0f, 1.0f, 0.0f, 0.0f));

	MATRIX shadowCPM[NUM_SPLITS];
	for (int i = 0; i < NUM_SPLITS; ++i) {
		// Bind and clear current cascade
		glBindFramebuffer(GL_FRAMEBUFFER, gameState->depthFbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gameState->shadowMaps[i], 0);
		glClear(GL_DEPTH_BUFFER_BIT);

		// Compute camera frustum slice boundary points in world space
		VECTOR frustumPoints[8];
		getFrustumPoints(f[i], gameState->position, viewDir, frustumPoints);
		MATRIX shadowMatrix;
		calculateCropMatrix(f[i], frustumPoints, lightView, shadowCPM + i);
		glUniformMatrix4fv(gameState->depthProgramMvp, 1, GL_FALSE, MatrixGet(mv, shadowCPM[i]));

		// Draw the model
		glEnableVertexAttribArray(gameState->depthProgramPosition);

		drawModelGeometry(gameState->objModel, gameState->depthProgramPosition);
		drawModelGeometry(gameState->groundModel, gameState->depthProgramPosition);

		glDisableVertexAttribArray(gameState->depthProgramPosition);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Main pass: render scene as normal with shadow mapping (using depth map)
	// glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);
	glViewport(0, 0, 800, 600);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear the screen

	// Draw the model
	glUseProgram(gameState->program);
	glUniformMatrix4fv(gameState->viewUniform, 1, GL_FALSE, MatrixGet(mv, gameState->view));

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
	glUniform3fv(glGetUniformLocation(gameState->program, "lightDir"), 1, VectorGet(vv, lightDir));

	glEnableVertexAttribArray(gameState->posAttrib);
	glEnableVertexAttribArray(gameState->normalAttrib);

	drawModelGeometryNormal(gameState->objModel, gameState->posAttrib, gameState->normalAttrib);
	drawModelGeometryNormal(gameState->groundModel, gameState->posAttrib, gameState->normalAttrib);

	glDisableVertexAttribArray(gameState->posAttrib);
	glDisableVertexAttribArray(gameState->normalAttrib);

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

	// Depth pass
	glBindFramebuffer(GL_FRAMEBUFFER, gameState->depthFbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gameState->depthTexture, 0);
	glClear(GL_DEPTH_BUFFER_BIT);
	glUseProgram(gameState->depthProgram);
	MATRIX mvp = MatrixMultiply(gameState->projection, MatrixMultiply(gameState->view, gameState->model));
	glUniformMatrix4fv(gameState->depthProgramMvp, 1, GL_FALSE, MatrixGet(mv, mvp));

	glEnableVertexAttribArray(gameState->depthProgramPosition);
	drawModelGeometry(gameState->objModel, gameState->depthProgramPosition);
	drawModelGeometry(gameState->groundModel, gameState->depthProgramPosition);
	glDisableVertexAttribArray(gameState->depthProgramPosition);

	// SSAO for realz lul
	glBindBuffer(GL_ARRAY_BUFFER, gameState->quadBuffer);

	// Compute camera space Z texture
	/*glBindFramebuffer(GL_FRAMEBUFFER, gameState->cszFramebuffers[0]);
	glClear(GL_COLOR_BUFFER_BIT);
	glUseProgram(gameState->reconstructCSZProgram);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gameState->depthTexture);
	glEnableVertexAttribArray(glGetAttribLocation(gameState->reconstructCSZProgram, "position"));
	glVertexAttribPointer(glGetAttribLocation(gameState->reconstructCSZProgram, "position"), 2, GL_FLOAT, GL_FALSE, 0, 0);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(glGetAttribLocation(gameState->reconstructCSZProgram, "position"));*/
	// Generate the other levels
	/*glUseProgram(gameState->minifyProgram);
	glEnableVertexAttribArray(glGetAttribLocation(gameState->minifyProgram, "position"));
	glVertexAttribPointer(glGetAttribLocation(gameState->minifyProgram, "position"), 2, GL_FLOAT, GL_FALSE, 0, 0);
	for (int i = 1; i <= SAO_MAX_MIP_LEVEL; ++i) {
		glBindFramebuffer(GL_FRAMEBUFFER, gameState->cszFramebuffers[i]);
		glClear(GL_COLOR_BUFFER_BIT);
		glBindTexture(GL_TEXTURE_2D, gameState->cszTexture);
		glUniform1f(glGetUniformLocation(gameState->minifyProgram, "previousMipNumber"), i - 1);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}
	glDisableVertexAttribArray(glGetAttribLocation(gameState->minifyProgram, "position"));*/

	glBindFramebuffer(GL_FRAMEBUFFER, gameState->ssaoFbo);
	glClear(GL_COLOR_BUFFER_BIT);
	glUseProgram(gameState->ssaoProgram);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gameState->depthTexture);

	glEnableVertexAttribArray(glGetAttribLocation(gameState->ssaoProgram, "position"));
	glVertexAttribPointer(glGetAttribLocation(gameState->ssaoProgram, "position"), 2, GL_FLOAT, GL_FALSE, 0, 0);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(glGetAttribLocation(gameState->ssaoProgram, "position"));

	glUseProgram(gameState->blurProgram);
	glEnableVertexAttribArray(glGetAttribLocation(gameState->blurProgram, "position"));
	glVertexAttribPointer(glGetAttribLocation(gameState->blurProgram, "position"), 2, GL_FLOAT, GL_FALSE, 0, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, gameState->blurFbo);
	glClear(GL_COLOR_BUFFER_BIT);
	glBindTexture(GL_TEXTURE_2D, gameState->ssaoTexture);
	glUniform2i(glGetUniformLocation(gameState->blurProgram, "axis"), 1, 0);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	glBindFramebuffer(GL_FRAMEBUFFER, gameState->saoResultFbo);
	glClear(GL_COLOR_BUFFER_BIT);
	glBindTexture(GL_TEXTURE_2D, gameState->blurTexture);
	glUniform2i(glGetUniformLocation(gameState->blurProgram, "axis"), 0, 1);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(glGetAttribLocation(gameState->blurProgram, "position"));

	glDisable(GL_SCISSOR_TEST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Draw GUI
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	// glBlendFunc(GL_ZERO, GL_SRC_COLOR);
	spriteBatchBegin(batch);
	spriteBatchDraw(batch, gameState->ssaoTexture, 0, 0, 800, 600);
	// spriteBatchDraw(batch, gameState->depthTexture, 0, 0, 800, 600);
	// widgetValidate(gameState->flexLayout, 800, 600);
	// widgetDraw(gameState->flexLayout, batch);
	spriteBatchEnd(batch);
	glDisable(GL_BLEND);
}

static void gameStateResize(struct State *state, int width, int height) {
	struct GameState *gameState = (struct GameState *) state;
	widgetLayout(gameState->flexLayout, width, MEASURE_EXACTLY, height, MEASURE_EXACTLY);

	gameState->projection = MatrixPerspective(90.f, (float) width / height, zNear, zFar);
	glUseProgram(gameState->program);
	ALIGN(16) float mv[16];
	glUniformMatrix4fv(gameState->projectionUniform, 1, GL_FALSE, MatrixGet(mv, gameState->projection));
}

static struct FlexParams params0 = { ALIGN_END, -1, 100, UNDEFINED, 20, 0, 20, 20 },
						 params2 = {ALIGN_CENTER, 1, 100, UNDEFINED, 0, 0, 0, 50},
						 params1 = { ALIGN_CENTER, 1, UNDEFINED, UNDEFINED, 0, 0, 0, 0 };

/**
 * Returns a random float between 0 and 1.
 */
static float randomFloat() {
	return rand() / RAND_MAX;
}

void gameStateInitialize(struct GameState *gameState, struct SpriteBatch *batch) {
	struct State *state = (struct State *) gameState;
	state->update = gameStateUpdate;
	state->draw = gameStateDraw;
	state->resize = gameStateResize;
	gameState->batch = batch;

	const GLchar *vertexShaderSource = "attribute vec3 position;"
		"attribute vec3 normal;"
		"uniform mat4 model;"
		"uniform mat4 view;"
		"uniform mat4 projection;"
		"const int NUM_CASCADES = 3;"
		"uniform mat4 lightMVP[NUM_CASCADES];"
		"varying vec4 lightSpacePos[NUM_CASCADES];"
		"varying vec3 vNormal;"
		"void main() {"
		"	vNormal = vec3(view * model * vec4(normal, 0.0));"
		"	vNormal = normal;"
		"	gl_Position = projection * view * model * vec4(position, 1.0);"
		"	for (int i = 0; i < NUM_CASCADES; ++i) {"
		"		lightSpacePos[i] = lightMVP[i] * vec4(position, 1.0);"
		"	}"
		"}",
		*fragmentShaderSource = "precision mediump float;"
			"varying vec3 vNormal;"
			"uniform vec3 lightDir;"
			"const int NUM_CASCADES = 3;"
			"varying vec4 lightSpacePos[NUM_CASCADES];"
			"uniform sampler2D shadowMap[NUM_CASCADES];"
			"uniform float cascadeEndClipSpace[NUM_CASCADES];"
			"float calcShadowFactor() {"
			"	for (int i = 0; i < NUM_CASCADES; ++i) {"
			// "	shadowCoord = vec4(0.5 * shadowCoord.xyz + 0.5, shadowCoord.w);"
			"	if (gl_FragCoord.z < cascadeEndClipSpace[i]) {"
			"	vec4 shadowCoord = lightSpacePos[i];"
			"	float z = shadowCoord.z;"
			"	float depth = texture2D(shadowMap[i], shadowCoord.xy).x;"
			"	return depth + 0.001 < z ? 0.3 : 1.0;" // Slight offset to prevent shadow acne
			"	}"
			"	}"
			"	return 1.0;"
			"}"
			"void main() {"
			"	float shadowFactor = calcShadowFactor();"
			"	float intensity = max(dot(normalize(vNormal), normalize(-lightDir)), 0.0);"
			"	gl_FragColor = vec4(shadowFactor * intensity * vec3(1.0, 1.0, 1.0), 1.0);"
			"}";
	GLuint program = createProgram(vertexShaderSource, fragmentShaderSource);
	gameState->program = program;

	// Specify the layout of the vertex data
	gameState->posAttrib = glGetAttribLocation(program, "position");
	gameState->normalAttrib = glGetAttribLocation(program, "normal");
	// Get the location of program uniforms
	GLint modelUniform = glGetUniformLocation(program, "model");
	GLint viewUniform = glGetUniformLocation(program, "view");
	GLint projectionUniform = glGetUniformLocation(program, "projection");
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
		*skyboxFragmentShaderSource = "precision mediump float;"
			"varying vec3 eyeDirection;"
			"uniform samplerCube texture;"
			"void main() {"
			"	gl_FragColor = textureCube(texture, eyeDirection);"
			"	gl_FragDepth = 1.0;" // TODO add replacement
			"}";
	gameState->skyboxProgram = createProgram(skyboxVertexShaderSource, skyboxFragmentShaderSource);
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
		"uniform mat4 mvp;" // lightMVP
		"void main() {"
		"	gl_Position = mvp * vec4(position, 1.0);"
		"}",
		*depthFragmentShaderSource = "void main() {}";
	gameState->depthProgram = createProgram(depthVertexShaderSource, depthFragmentShaderSource);
	gameState->depthProgramPosition = glGetAttribLocation(gameState->depthProgram, "position");
	gameState->depthProgramMvp = glGetUniformLocation(gameState->depthProgram, "mvp");

	// Create the depth buffers
	glGenTextures(NUM_SPLITS, gameState->shadowMaps);
	for (int i = 0; i < NUM_SPLITS; ++i) {
		glBindTexture(GL_TEXTURE_2D, gameState->shadowMaps[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, DEPTH_SIZE, DEPTH_SIZE, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		/*glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
		  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);*/
	}

	// Create the FBO
	glGenFramebuffers(1, &gameState->depthFbo);
	glBindFramebuffer(GL_FRAMEBUFFER, gameState->depthFbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gameState->shadowMaps[0], 0);
	// glDrawBuffer(GL_NONE); // Disable writes to the color buffer
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		printf("Error creating framebuffer.\n");
	}

	// SSAO
	// Clipping plane constants for use by reconstructZ
	const float clipInfo[] = {zNear * zFar, zNear - zFar, zFar};

	glGenTextures(1, &gameState->depthTexture);
	glBindTexture(GL_TEXTURE_2D, gameState->depthTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 800, 600, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glGenTextures(1, &gameState->cszTexture);
	glBindTexture(GL_TEXTURE_2D, gameState->cszTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // GL_NEAREST_MIPMAP_NEAREST
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, SAO_MAX_MIP_LEVEL);
	glGenFramebuffers(SAO_MAX_MIP_LEVEL, gameState->cszFramebuffers);
	/*for (int i = 0; i <= SAO_MAX_MIP_LEVEL && i == 0; ++i) {
		glTexImage2D(GL_TEXTURE_2D, i, GL_R32F, 800, 600, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
		glBindFramebuffer(GL_FRAMEBUFFER, gameState->cszFramebuffers[i]);
		// glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, gameState->cszTexture, i);
	}*/
	// glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 800, 600, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 800, 600, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glBindFramebuffer(GL_FRAMEBUFFER, gameState->cszFramebuffers[0]);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gameState->cszTexture, 0);

	const char *reconstructVertexShaderSource = "attribute vec2 position;"
		"varying vec2 vTexCoord;"
		"void main() {"
		"	gl_Position = vec4(position, 0.0, 1.0);"
		"	vTexCoord = 0.5 * position + 0.5;"
		"}",
		*reconstructFragmentShaderSource = "varying vec2 vTexCoord;"
			"uniform sampler2D depthBuffer;"
			/*
			 * Clipping plane constants for use by reconstructZ
			 * clipInfo = (z_f == -inf()) ? Vector3(z_n, -1.0f, 1.0f) : Vector3(z_n * z_f,  z_n - z_f,  z_f);
			 */
			"uniform vec3 clipInfo;"
			"float reconstructCSZ(float d) {"
			"	return clipInfo[0] / (clipInfo[1] * d + clipInfo[2]);"
			"}"
			"void main() {"
			// "	gl_FragColor.r = reconstructCSZ(texture2D(depthBuffer, vTexCoord).r);"
			"	gl_FragColor = vec4(vec3((2.0 * 1.0) / (100.0 + 1.0 - texture2D(depthBuffer, vTexCoord).r * (100.0 - 1.0))), 1.0);"
			// "	gl_FragColor = vec4(vTexCoord, 0.0, 1.0);"
			"}";
	gameState->reconstructCSZProgram = createProgram(reconstructVertexShaderSource, reconstructFragmentShaderSource);
	glUseProgram(gameState->reconstructCSZProgram);
	glUniform3fv(glGetUniformLocation(gameState->reconstructCSZProgram, "clipInfo"), 1, clipInfo);

	/*const char *minifyVertexShaderSource = "attribute vec2 position;"
		"void main() {"
		"	gl_Position = vec4(position, 0.0, 1.0);"
		"}",
		*minifyFragmentShaderSource = "#extension GL_EXT_gpu_shader4 : require\n"
			"uniform sampler2D texture;"
			"uniform int previousMipNumber;"
			"void main() {"
			"	ivec2 ssP = ivec2(gl_FragCoord.xy);"
			"	gl_FragColor = texelFetch2D(texture, clamp(ssP * 2 + ivec2(ssP.y & 1, ssP.x & 1), ivec2(0), textureSize2D(texture, previousMipNumber) - 1), previousMipNumber);"
			"}";
	gameState->minifyProgram = createProgram(minifyVertexShaderSource, minifyFragmentShaderSource);

	float ssaoKernelSource[3 * 64];
	for (int i = 0; i < 64; ++i) {
		VECTOR sample = VectorSet(
				2.0f * randomFloat() - 1.0f,
				2.0f * randomFloat() - 1.0f,
				randomFloat(),
				0);
		// sample = Vector4Normalize(sample);
		float scale = i / 64.0f;
		scale = 0.1f + 0.9f * scale * scale;
		sample = VectorMultiply(VectorReplicate(scale), sample);
		VectorGet(vv, sample);
		ssaoKernelSource[3 * i] = vv[0];
		ssaoKernelSource[3 * i + 1] = vv[1];
		ssaoKernelSource[3 * i + 2] = vv[2];
	}
	float ssaoNoise[3 * 16];
	for (int i = 0; i < 16; ++i) {
		ssaoNoise[3 * i] = 2 * randomFloat() - 1;
		ssaoNoise[3 * i + 1] = 2 * randomFloat() - 1;
		ssaoNoise[3 * i + 2] = 0.0f;
	}
	glGenTextures(1, &gameState->ssaoNoiseTexture);
	glBindTexture(GL_TEXTURE_2D, gameState->ssaoNoiseTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 4, 4, 0, GL_RGB, GL_FLOAT, ssaoNoise);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);*/

	glGenTextures(1, &gameState->ssaoTexture);
	glBindTexture(GL_TEXTURE_2D, gameState->ssaoTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 800, 600, 0, GL_RGB, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glGenFramebuffers(1, &gameState->ssaoFbo);
	glBindFramebuffer(GL_FRAMEBUFFER, gameState->ssaoFbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gameState->ssaoTexture, 0);

	const char *ssaoVertexShaderSource = "attribute vec2 position;"
		"void main() {"
		"	gl_Position = vec4(position, 0.0, 1.0);"
		"}",
		*ssaoFragmentShaderSource = "#extension GL_EXT_gpu_shader4 : require\n"
			// "#extension GL_ARB_gpu_shader5 : enable\n"
			// "#extension GL_OES_standard_derivatives : enable\n"
			"#define NUM_SAMPLES (256)\n" // 11
			"#define FAR_PLANE_Z (-100.0)\n"
			"#define NUM_SPIRAL_TURNS (11)\n" // 7
			"#define LOG_MAX_OFFSET (3)\n"
			"#define MAX_MIP_LEVEL (5)\n"
			"uniform float radius;"
			"uniform float projScale;"
			"uniform sampler2D csZBuffer;"
			"uniform float bias;"
			"uniform float intensityDivR6;"
			"uniform vec3 clipInfo;"
			"uniform vec4 projInfo;"
			// Tile noise texture over screen based on screen dimensions divided by noise size
			// "uniform vec2 noiseScale;"
			// "uniform vec3 sampleKernel[64];"
			/*
			 * Reconstruct camera-space P.xyz from screen-space S = (x, y) in
			 * pixels and camera-space z < 0.  Assumes that the upper-left pixel center
			 * is at (0.5, 0.5) (but that need not be the location at which the sample tap
			 * was placed!)
			 */
			"vec3 reconstructCSPosition(vec2 S, float z) {"
			"	return vec3((S * projInfo.xy + projInfo.zw) * z, z);"
			"}"
			// Reconstructs screen-space unit normal from screen-space position
			"vec3 reconstructCSFaceNormal(vec3 C) {"
			"	return normalize(cross(dFdy(C), dFdx(C)));"
			"}"
			"vec3 reconstructNonUnitCSFaceNormal(vec3 C) {"
			"	return cross(dFdy(C), dFdx(C));"
			"}"
			/* Returns a unit vector and a screen-space radius for the tap on a unit disk (the caller should scale by the actual disk radius) */
			"vec2 tapLocation(int sampleNumber, float spinAngle, out float ssR) {"
			"	float alpha = float(sampleNumber + 0.5) * (1.0 / NUM_SAMPLES);" // Radius relative to ssR
			"	float angle = alpha * (NUM_SPIRAL_TURNS * 6.28) + spinAngle;"
			"	ssR = alpha;"
			"	return vec2(cos(angle), sin(angle));"
			"}"
			"float CSZToKey(float z) {"
			"	return clamp(z * (1.0 / FAR_PLANE_Z), 0.0, 1.0);"
			"}"
			"void packKey(float key, out vec2 p) {"
			"	float temp = floor(key * 256.0);" // Round to nearest 1 / 256.0
			"	p.x = temp * (1.0 / 256.0);" // Integer part
			"	p.y = key * 256.0 - temp;" // Fractional part
			"}"
			"float reconstructCSZ(float d) {"
			"	return clipInfo[0] / (clipInfo[1] * d + clipInfo[2]);"
			"}"
			/* Read the camera-space position of the point at screen-space pixel ssP */
			"vec3 getPosition(ivec2 ssP) {"
			"	return reconstructCSPosition(vec2(ssP) + 0.5, reconstructCSZ(texelFetch2D(csZBuffer, ssP, 0).r));"
			"}"
			/* Read the camera-space position of the point at screen-space pixel ssC + unitOffset * ssR. Assumes length(unitOffset) == 1 */
			"vec3 getOffsetPosition(ivec2 ssC, vec2 unitOffset, float ssR) {\n"
			// "#ifdef GL_EXT_gpu_shader5\n"
			// "	int mipLevel = clamp(findMSB(int(ssR)) - LOG_MAX_OFFSET, 0, MAX_MIP_LEVEL);\n"
			// "#else\n"
			// "	int mipLevel = clamp(int(floor(log2(ssR))) - LOG_MAX_OFFSET, 0, MAX_MIP_LEVEL);\n"
			// "#endif\n"
			// "	mipLevel = 0;" // don't use mipmaps
			"	ivec2 ssP = ivec2(ssR * unitOffset) + ssC;"
			"	return getPosition(ssP);"
			// "	ivec2 mipP = clamp(ssP >> mipLevel, ivec2(0), textureSize2D(csZBuffer, mipLevel) - ivec2(1));"
			// "	return reconstructCSPosition(vec2(ssP) + 0.5, texelFetch2D(csZBuffer, mipP, mipLevel).r);"
			"}"
			"float radius2 = radius * radius;"
			"float sampleAO(in ivec2 ssC, in vec3 C, in vec3 n_C, in float ssDiskRadius, in int tapIndex, in float randomPatternRotationAngle) {"
			"	float ssR;"
			"	vec2 unitOffset = tapLocation(tapIndex, randomPatternRotationAngle, ssR);"
			"	ssR *= ssDiskRadius;"
			"	vec3 Q = getOffsetPosition(ssC, unitOffset, ssR);"
			"	vec3 v = Q - C;"
			"	float vv = dot(v, v), vn = dot(v, n_C);"
			"	const float epsilon = 0.01;"
			"	float f = max(radius2 - vv, 0.0);"
			"	return f * f * f * max((vn - bias) / (epsilon + vv), 0.0);"
			"}"
			"void main() {"
			"	ivec2 ssC = ivec2(gl_FragCoord.xy);" // Pixel being shaded
			"	vec3 C = getPosition(ssC);" // World space point being shaded
			"	packKey(CSZToKey(C.z), gl_FragColor.gb);"
			"	if (C.z < FAR_PLANE_Z) discard;"
			"	float randomPatternRotationAngle = (3 * ssC.x ^ ssC.y + ssC.x * ssC.y) * 10;"
			"	vec3 n_C = reconstructCSFaceNormal(C);"
			"	float ssDiskRadius = -projScale * radius / C.z;" // -projScale
			"	float sum = 0.0;"
			"	for (int i = 0; i < NUM_SAMPLES; ++i) {"
			"		sum += sampleAO(ssC, C, n_C, ssDiskRadius, i, randomPatternRotationAngle);"
			"	}"
			"	float A = max(0.0, 1.0 - sum * intensityDivR6 * (5.0 / NUM_SAMPLES));"
			"	if (abs(dFdx(C.z)) < 0.02) A -= dFdx(A) * ((ssC.x & 1) - 0.5);"
			"	if (abs(dFdy(C.z)) < 0.02) A -= dFdy(A) * ((ssC.y & 1) - 0.5);"
			"	gl_FragColor.r = A;"
			"}";
	gameState->ssaoProgram = createProgram(ssaoVertexShaderSource, ssaoFragmentShaderSource);
	glUseProgram(gameState->ssaoProgram);
	// glUniform1f(glGetUniformLocation(gameState->ssaoProgram, "far"), zFar);
	// glUniform1f(glGetUniformLocation(gameState->ssaoProgram, "near"), zNear);
	const float radius = 2.0f;
	glUniform1f(glGetUniformLocation(gameState->ssaoProgram, "radius"), radius);
	// glUniform2f(glGetUniformLocation(gameState->ssaoProgram, "noiseScale"), 800 / 4, 600 / 4);
	// glUniform3fv(glGetUniformLocation(gameState->ssaoProgram, "sampleKernel"), 64, ssaoKernelSource);
	// glUniform1f(glGetUniformLocation(gameState->ssaoProgram, "normalTexture"), 0);
	// glUniform1f(glGetUniformLocation(gameState->ssaoProgram, "noiseTexture"), 2);
	// glUniform1f(glGetUniformLocation(gameState->ssaoProgram, "aspectRatio"), 800 / 600);
	// glUniform1f(glGetUniformLocation(gameState->ssaoProgram, "tanHalfFov"), tanf(DEGREES_TO_RADIANS(90.f / 2.0f)));
	glUniform1f(glGetUniformLocation(gameState->ssaoProgram, "bias"), 0.012f); // 0.012f
	const float intensity = 1;
	glUniform1f(glGetUniformLocation(gameState->ssaoProgram, "intensityDivR6"), intensity / pow(radius, 6.0f));
	float projScale = 800.0f / (-2.0f * tanf(DEGREES_TO_RADIANS(0.5f * 90.0f)));
	printf("projScale: %f\n", projScale);
	glUniform1f(glGetUniformLocation(gameState->ssaoProgram, "projScale"), projScale);
	glUniform3fv(glGetUniformLocation(gameState->ssaoProgram, "clipInfo"), 1, clipInfo);
	MatrixGet(mv, gameState->projection);
	glUniform4f(glGetUniformLocation(gameState->ssaoProgram, "projInfo"), 2.0f / (800.0f * mv[0]), 2.0f / (600.0f * mv[5]), -1.0f / mv[0], -1.0f / mv[5]);

	const char *blurVertexShaderSource = "attribute vec2 position;"
		"void main() {"
		"	gl_Position = vec4(position, 0.0, 1.0);"
		"}",
		*blurFragmentShaderSource = "#define EDGE_SHARPNESS (1.0)\n" // 1.0
		"#define SCALE (2)\n"
		"#define R (4)\n"
		"uniform sampler2D source;"
		"uniform ivec2 axis;"
		/* Returns a number on (0, 1) */
		"float unpackKey(vec2 p) {"
		"	return p.x * (256.0 / 257.0) + p.y * (1.0 / 257.0);"
		"}"
		"void main() {"
		"	float gaussian[R + 1];"
		"	gaussian[0] = 0.153170; gaussian[1] = 0.144893; gaussian[2] = 0.122649; gaussian[3] = 0.092902; gaussian[4] = 0.062970;"
		"	ivec2 ssC = ivec2(gl_FragCoord.xy);"
		"	vec4 temp = texelFetch2D(source, ssC, 0);"
		"	float key = unpackKey(gl_FragColor.gb = temp.gb);"
		"	float sum = temp.r;"
		/*"	if (key == 1.0) {"
		"		gl_FragColor.r = sum;"
		"		return;"
		"	}"*/
		// Base weight for depth falloff. Increase this for better edge discrimination, decrease for more blurriness
		"	float totalWeight = gaussian[0];"
		"	sum *= totalWeight;"
		"	for (int r = -R; r <= R; ++r) {"
		"		if (r != 0) {"
		"			temp = texelFetch2D(source, ssC + axis * (r * SCALE), 0);"
		"			float tapKey = unpackKey(temp.gb);"
		"			float value = temp.r;"
		"			float weight = 0.3 * gaussian[abs(r)];"
		"			weight *= max(0.0, 1.0 - (EDGE_SHARPNESS * 2000.0) * abs(tapKey - key));"
		"			sum += value * weight;"
		"			totalWeight += weight;"
		"		}"
		"	}"
		"	const float epsilon = 0.0001;"
		"	gl_FragColor.r = sum / (totalWeight + epsilon);"
		"}";
	gameState->blurProgram = createProgram(blurVertexShaderSource, blurFragmentShaderSource);

	glGenTextures(1, &gameState->blurTexture);
	glBindTexture(GL_TEXTURE_2D, gameState->blurTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 800, 600, 0, GL_RGB, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glGenFramebuffers(1, &gameState->blurFbo);
	glBindFramebuffer(GL_FRAMEBUFFER, gameState->blurFbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gameState->blurTexture, 0);
	glGenFramebuffers(1, &gameState->saoResultFbo);
	glBindFramebuffer(GL_FRAMEBUFFER, gameState->saoResultFbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gameState->ssaoTexture, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0); // TODO remove

	float quadVertices[] = {
		-1.0f, -1.0f,
		1.0f, -1.0f,
		-1.0f, 1.0f,
		-1.0f, 1.0f,
		1.0f, -1.0f,
		1.0f, 1.0f
	};
	glGenBuffers(1, &gameState->quadBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, gameState->quadBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof quadVertices, quadVertices, GL_STATIC_DRAW);

	gameState->position = VectorSet(0, 0, 0, 1);
	gameState->yaw = 0;
	gameState->pitch = 0;
	gameState->objModel = loadModelFromObj("bunny.obj");
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
	/*glDeleteProgram(gameState->skyboxProgram);
	glDeleteTextures(1, &gameState->skyboxTexture);
	glDeleteBuffers(1, &gameState->skyboxVertexBuffer);
	glDeleteBuffers(1, &gameState->skyboxIndexBuffer);*/

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
