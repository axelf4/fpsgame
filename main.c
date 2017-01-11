#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <SDL.h>
#include <gl/glew.h>
#include <SDL_opengl.h>
#include <vmath.h>
#include <png.h>
#include <xmmintrin.h>

#include "model.h"
#include "widget.h"
#include "renderer.h"
#include "glUtil.h"

#define MOUSE_SENSITIVITY 0.006f
#define MOVEMENT_SPEED .05f

static GLenum getGLColorFormat(const int color_type) {
	switch (color_type) {
		case PNG_COLOR_TYPE_RGB:
			return GL_RGB;
		case PNG_COLOR_TYPE_RGB_ALPHA:
			return GL_RGBA;
		case PNG_COLOR_TYPE_GRAY:
			return GL_LUMINANCE;
		case PNG_COLOR_TYPE_GRAY_ALPHA:
			return GL_LUMINANCE_ALPHA;
		default:
			fprintf(stderr, "Unknown libpng color type %d.\n", color_type);
			return 0;
	}
}

GLuint loadPNGTexture(const char *filename, png_uint_32 *width, png_uint_32 *height) {
	FILE *fp = fopen(filename, "rb");
	if (!fp) {
		perror(filename);
		return 0;
	}
	// Create png struct
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) {
		fprintf(stderr, "Error: png_create_read_struct returned 0.\n");
		fclose(fp);
		return 0;
	}
	// Create png info struct
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		fprintf(stderr, "Error: png_create_info_struct returned 0.\n");
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		fclose(fp);
		return 0;
	}
	// Set up error handling
	if (setjmp(png_jmpbuf(png_ptr))) {
		fprintf(stderr, "Error from libPNG when reading png_ptr file.\n");
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		fclose(fp);
		return 0;
	}
	// Set up PNG reading
	png_init_io(png_ptr, fp);
	png_read_info(png_ptr, info_ptr); // Read information up to the image data
	int bit_depth, color_type;
	png_get_IHDR(png_ptr, info_ptr, width, height, &bit_depth, &color_type, NULL, NULL, NULL);
	// Convert transparency to full alpha
	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
		png_set_tRNS_to_alpha(png_ptr);
	}
	// Convert grayscale
	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
		png_set_expand_gray_1_2_4_to_8(png_ptr);
	}
	// Convert paletted images
	if (color_type == PNG_COLOR_TYPE_PALETTE) {
		png_set_palette_to_rgb(png_ptr);
	}
	// Add alpha channel (GL_RGBA is faster than GL_RGB on many GPUs)
	if (color_type == PNG_COLOR_TYPE_PALETTE || color_type == PNG_COLOR_TYPE_RGB) {
		png_set_add_alpha(png_ptr, 0xFF, PNG_FILLER_AFTER);
	}
	// Ensure 8-bit packing
	if (bit_depth < 8) {
		png_set_packing(png_ptr);
	} else if (bit_depth == 16) {
		png_set_scale_16(png_ptr);
	}
	png_read_update_info(png_ptr, info_ptr);
	color_type = png_get_color_type(png_ptr, info_ptr);
	GLint format = getGLColorFormat(color_type);

	png_size_t rowbytes = png_get_rowbytes(png_ptr, info_ptr);
	rowbytes += 3 - ((rowbytes - 1) % 4); // glTexImage2d requires rows to be 4-byte aligned
	png_byte *imageData = malloc(sizeof(png_byte) * rowbytes * *height);
	if (!imageData) {
		fprintf(stderr, "error: could not allocate memory for png_ptr image data.\n");
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		fclose(fp);
		return 0;
	}
	png_bytepp rowPointers = malloc(sizeof(png_bytep) * *height);
	if (!rowPointers) {
		fprintf(stderr, "error: could not allocate memory for row pointers.\n");
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		free(imageData);
		fclose(fp);
		return 0;
	}
	// Point the row pointers at the correct offsets of imageData
	for (unsigned int i = 0; i < *height; ++i) {
		// rowPointers[*height - 1 - i] = imageData + i * rowbytes;
		rowPointers[i] = imageData + i * rowbytes;
	}
	// Read the image data through rowPointers
	png_read_image(png_ptr, rowPointers);

	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, format, *width, *height, 0, format, GL_UNSIGNED_BYTE, imageData);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// clean up
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	free(imageData);
	free(rowPointers);
	fclose(fp);
	return texture;
}

int main(int argc, char *arcv[]) {
	setvbuf(stdout, 0, _IONBF, 0);
	setvbuf(stderr, 0, _IONBF, 0);
	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		printf("SDL_Init Error: %s\n", SDL_GetError());
		return 1;
	}
	SDL_SetRelativeMouseMode(SDL_TRUE); // Capture mouse and use relative coordinates
	SDL_Window *window;
	if (!(window = SDL_CreateWindow("Hello", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE))) {
		fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
		SDL_Quit();
		return 1;
	}
	SDL_GLContext glcontext = SDL_GL_CreateContext(window);
	GLenum err = glewInit();
	if (err != GLEW_OK) {
		printf("glewInit Error: %s\n", glewGetErrorString(err));
	}

	glEnable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glClearColor(0.3, 0.7, 0.1, 1.0);

	ALIGN(16) float vv[4], mv[16];

	const char objPath[] = "../cube.obj";
	struct Model *model = loadModelFromObj(objPath);

	const GLchar *vertexShaderSource = "#version 150 core\n"
		"in vec3 position;"
		"uniform mat4 model;"
		"uniform mat4 view;"
		"uniform mat4 projection;"
		"void main() {"
		"	gl_Position = projection * model * view * vec4(position, 1.0);"
		"}";
	const GLchar *fragmentShaderSource = "#version 150 core\n"
		"out vec4 outColor;"
		"void main() {"
		"	outColor = vec4(1.0, 0.0, 0.0, 1.0);"
		"}";
	GLuint program = createProgram(vertexShaderSource, fragmentShaderSource);
	glBindFragDataLocation(program, 0, "outColor");
	glLinkProgram(program);

	// Specify the layout of the vertex data
	GLint posAttrib = glGetAttribLocation(program, "position");
	// Get the location of program uniforms
	GLint modelUniform = glGetUniformLocation(program, "model");
	GLint viewUniform = glGetUniformLocation(program, "view");
	GLint projectionUniform = glGetUniformLocation(program, "projection");

	glUseProgram(program);
	// TODO rename to model, view, projection
	MATRIX modelMatrix = MatrixIdentity(),
		   viewMatrix,
		   projectionMatrix = MatrixPerspective(90.f, 800.0f / 600.0f, 1.0f, 3000.0f);
	glUniformMatrix4fv(modelUniform, 1, GL_FALSE, MatrixGet(mv, modelMatrix));
	glUniformMatrix4fv(projectionUniform, 1, GL_FALSE, MatrixGet(mv, projectionMatrix));

	// GUI code

	struct Font *font = loadFont("../Vera.ttf", 512, 512);

	/*hb_buffer_t *buffer = hb_buffer_create(); // Create a buffer for HarfBuzz to use.
	const char language[] = "en";
	hb_buffer_set_language(buffer, hb_language_from_string(language, strlen(language)));
	hb_buffer_add_utf8(buffer, text, strlen(text), 0, strlen(text));
	hb_buffer_guess_segment_properties(buffer);
	hb_shape(font->hb_ft_font, buffer, NULL, 0);
	unsigned int glyphCount;
	hb_glyph_info_t *glyphInfo = hb_buffer_get_glyph_infos(buffer, &glyphCount);
	hb_glyph_position_t *glyphPos = hb_buffer_get_glyph_positions(buffer, &glyphCount);*/

	// rect drawing
	png_uint_32 width, height;
	GLuint cat = loadPNGTexture("../cat.png", &width, &height);
	if (!cat) {
		fprintf(stderr, "Failed to load png image.\n");
	}
	struct SpriteRenderer *spriteRenderer = spriteRendererCreate(1);
	struct TextRenderer *textRenderer = textRendererCreate();

	FlexLayout flexLayout;
	flexLayoutInitialize(&flexLayout, DIRECTION_ROW, ALIGN_START);

	TestWidget testWidget;
	testWidgetInitialize(&testWidget, cat);
	struct FlexParams params0 = {
		ALIGN_CENTER, -1, UNDEFINED, UNDEFINED, 0, 0, 0, 0
	};
	testWidget.widget.layoutParams = &params0;
	containerAddChild((struct Container *) &flexLayout, (struct Widget *) &testWidget);

	TestWidget testWidget2;
	testWidgetInitialize(&testWidget2, cat);
	struct FlexParams params1 = {
		ALIGN_CENTER, 1, UNDEFINED, UNDEFINED, 0, 0, 0, 0
	};
	testWidget2.widget.layoutParams = &params1;
	containerAddChild((struct Container *) &flexLayout, (struct Widget *) &testWidget2);

	((struct Widget *) &flexLayout)->vtable->layout((struct  Widget *) &flexLayout, 800, MEASURE_EXACTLY, 600, MEASURE_EXACTLY);
	// end of GUI code

	VECTOR position = VectorSet(0, 0, 0, 1);
	float yaw = 0, pitch = 0;

	const Uint8 *state = SDL_GetKeyboardState(NULL);
	int running = 1;
	SDL_Event event;
	while (running) {
		while (SDL_PollEvent(&event)) if (event.type == SDL_QUIT) running = 0;
		if (state[SDL_SCANCODE_ESCAPE]) running = 0;

		VECTOR forward, up, right;
		forward = VectorSet(MOVEMENT_SPEED * sin(yaw), 0, MOVEMENT_SPEED * cos(yaw), 0);
		up = VectorSet(0, 1, 0, 0);
		right = Vector3Cross(forward, up);
		if (state[SDL_SCANCODE_W]) position = VectorAdd(position, forward);
		if (state[SDL_SCANCODE_A]) position = VectorAdd(position, right);
		if (state[SDL_SCANCODE_S]) position = VectorSubtract(position, forward);
		if (state[SDL_SCANCODE_D]) position = VectorSubtract(position, right);
		if (state[SDL_SCANCODE_LSHIFT]) position = VectorAdd(position, VectorSet(0, MOVEMENT_SPEED, 0, 0));
		if (state[SDL_SCANCODE_SPACE]) position = VectorSubtract(position, VectorSet(0, MOVEMENT_SPEED, 0, 0));

		int x, y;
		Uint32 button = SDL_GetRelativeMouseState(&x, &y);
		yaw += x * MOUSE_SENSITIVITY;
		pitch -= y * MOUSE_SENSITIVITY;

		// Clear the screen
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Draw the model
		glUseProgram(program);
		viewMatrix = MatrixTranslationFromVector(position);
		MATRIX rotationViewMatrix = MatrixRotationQuaternion(QuaternionRotationRollPitchYaw(pitch, yaw, 0));
		viewMatrix = MatrixMultiply(rotationViewMatrix, viewMatrix);
		viewMatrix = MatrixInverse(viewMatrix);
		glUniformMatrix4fv(viewUniform, 1, GL_FALSE, MatrixGet(mv, viewMatrix));

		glBindBuffer(GL_ARRAY_BUFFER, model->vertexBuffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model->indexBuffer);
		glEnableVertexAttribArray(posAttrib);
		glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glDrawElements(GL_TRIANGLES, model->indexCount, GL_UNSIGNED_INT, 0);
		glDisableVertexAttribArray(posAttrib);

		// Draw sprites
		spriteRenderer->projectionMatrix = MatrixOrtho(0, 800, 0, 600, -1, 1);
		spriteRendererBegin(spriteRenderer);

		((struct Widget *) &flexLayout)->vtable->draw((struct Widget *) &flexLayout, spriteRenderer);

		Color color = {1, 0, 0, 1};
		textRendererDraw(textRenderer, spriteRenderer, font, "Hello.", color, 0, 400);

		spriteRendererEnd(spriteRenderer);

		SDL_GL_SwapWindow(window);
	}
	SDL_HideWindow(window);

	containerDestroy((struct Container *) &flexLayout);
	glDeleteTextures(1, &cat);
	spriteRendererDestroy(spriteRenderer);
	fontDestroy(font);
	glDeleteProgram(program);
	destroyModel(model);

	SDL_Quit();

	return 0;
}
