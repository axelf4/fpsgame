#include "glUtil.h"
#include <stdio.h>

GLuint compileShader(GLenum type, const GLchar *source) {
	GLuint shader = glCreateShader(type);
	if (shader == 0) {
		fprintf(stderr, "Error creating shader.\n");
		return 0;
	}
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);
	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status); // Check shader compile status
	if (status == GL_FALSE) {
		GLint i;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &i);
		char infoLog[i];
		glGetShaderInfoLog(shader, i, NULL, infoLog);
		const char *typeName = type == GL_VERTEX_SHADER ? "GL_VERTEX_SHADER" : "GL_FRAGMENT_SHADER";
		fprintf(stderr, "%s: %s\n", typeName, infoLog);
		return 0;
	}
	return shader;
}

GLuint createProgram(const GLchar *vertexShaderSource, const GLchar *fragmentShaderSource) {
	GLuint program = 0, vertexShader, fragmentShader;
	if (!(vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource))) {
		goto deleteVertex;
	}
	if (!(fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource))) {
		goto deleteFragment;
	}
	// Mark the shaders for linkage into the program
	program = glCreateProgram();
	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);

deleteFragment:
	glDeleteShader(fragmentShader);
deleteVertex:
	glDeleteShader(vertexShader);

	return program;
}
