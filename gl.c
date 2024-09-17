#include "gl.h"


static SDL_GLContext __glcontext = NULL;

static void __log_err(const char *msg, const char *adtl) {
	if (adtl == NULL) {
		printf("[ERROR] %s\n", msg);
	} else {
		printf("[ERROR] %s:\n       %s\n", msg, adtl);
	}
	fflush(stdout);
	exit(1);
}

static void __log_warn(const char *msg, const char *adtl) {
	if (adtl == NULL) {
		printf("[WARN ] %s\n", msg);
	} else {
		printf("[WARN ] %s:\n       %s\n", msg, adtl);
	}
	fflush(stdout);
}

static void __log_info(const char *msg, const char *adtl) {
	if (adtl == NULL) {
		printf("[INFO ] %s\n", msg);
	} else {
		printf("[INFO ] %s:\n       %s\n", msg, adtl);
	}
	fflush(stdout);
}

static void __ensure_init() {
	if (__glcontext != NULL) return;
	__log_err("Tried to use gl without initialising!", "Aborting...");
}


void gl_init(int major, int minor, SDL_Window *window) {
	if (__glcontext != NULL) return;

	// Set OpenGL Settings
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	__glcontext = SDL_GL_CreateContext(window);
	if (__glcontext == NULL) {
		__log_err("Failed to create GL Context", SDL_GetError());
	}

	// Init GLEW
	glewExperimental = GL_FALSE;
	GLenum glerr = glewInit();
	if (glerr != GLEW_OK) gl_check_err("Failed to initialise GLEW");

	// Set VSync on
	if (SDL_GL_SetSwapInterval(1) < 0) __log_err("Failed to set VSync", SDL_GetError());

	// Set colour buffer's default values
	glClearColor( 0.1, 0.2, 0.3, 1.0 );
	gl_check_err("Failed to set colour buffer default values");
}

void gl_term() {
	if (__glcontext == NULL) return;

	// Destroy the context
	SDL_GL_DeleteContext(__glcontext);
}

GLuint gl_load_shader(GLenum type, const char *source_filename) {
	__ensure_init();

	// Open source file
	FILE *f = fopen(source_filename, "r");
	if (f == NULL) __log_err("Failed to open shader source", source_filename);

	// Create shader
	GLuint shader = glCreateShader(type);
	if (shader == 0) {
		fclose(f);
		gl_check_err("Failed to create new shader");
	}

	// Parse lines out of the file
	GLchar *lines[MAX_SHADER_LINES];
	GLint line_lens[MAX_SHADER_LINES];
	GLsizei line_count = 0;
	char curr_line[MAX_SHADER_COLS];
	size_t curr_len = 0;

	while (!feof(f) && line_count <= MAX_SHADER_LINES) {
		int ch = fgetc(f);
		if (feof(f)) break;

		curr_line[curr_len++] = ch;
		if (ch != '\n') continue;

		if (curr_len == 0) continue;
		line_lens[line_count] = curr_len;
		lines[line_count] = SDL_malloc(sizeof(char) * curr_len);
		SDL_memcpy(lines[line_count], curr_line, curr_len);
		line_count++;
		curr_len = 0;
	}

	// Finish parsing last line
	if (curr_len > 0) {
		line_lens[line_count] = curr_len;
		lines[line_count] = SDL_malloc(sizeof(char) * curr_len);
		SDL_memcpy(lines[line_count], curr_line, curr_len);
		line_count++;
	}
	fclose(f);

	// DEBUG: Print lines
	//printf("Successfully parsed %i lines from '%s':\n", line_count, source_filename);
	//for (int i=0; i<line_count; i++) {
	//	printf("  [% 4i](%i): %.*s\n", i+1, line_lens[i], line_lens[i], lines[i]);
	//}

	// Set Source and clean up memory
	glShaderSource(shader, line_count, (const GLchar *const *) lines, line_lens);
	gl_check_err("Failed to add shader source");
	for (int i=0; i<line_count; i++) SDL_free(lines[i]);

	// Compile Shader
	glCompileShader(shader);
	gl_check_err("Failed to compile shader");

	GLint compileStatus = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);
	if (compileStatus != GL_TRUE) {
		__log_warn("Shader compilation was unsuccessful:", NULL);
		gl_print_shader_log(shader);
	}
	__log_info("Successfully compiled shader", source_filename);

	return shader;
}

void gl_link_program(GLuint program) {
	__ensure_init();

	glLinkProgram(program);
	gl_check_err("Failed to link program");

	GLint succ = GL_TRUE;
	glGetProgramiv(program, GL_LINK_STATUS, &succ);
	if(succ != GL_TRUE) {
		__log_warn("Program linking was unsuccessful:", NULL);
		gl_print_prog_log(program);
	}
}

gl_frametex gl_create_frametex(GLuint width, GLuint height) {
	gl_frametex ftex;
	ftex.w = width;
	ftex.h = height;

	// Create empty texture the size of the screen
	glCreateTextures(GL_TEXTURE_2D, 1, &ftex.tex);
	glTextureStorage2D(ftex.tex, 1, GL_RGBA8, width, height);
	gl_check_err("Failed to upload frame-texture");

	// Set texture parameters
	glTextureParameteri(ftex.tex, GL_TEXTURE_BASE_LEVEL, 0);
	glTextureParameteri(ftex.tex, GL_TEXTURE_MAX_LEVEL, 0);
	glTextureParameteri(ftex.tex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTextureParameteri(ftex.tex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(ftex.tex, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTextureParameteri(ftex.tex, GL_TEXTURE_WRAP_T, GL_REPEAT);
	gl_check_err("Failed to set frame-texture parameters");

	// Create a new framebuffer and attach the texture to it
	glCreateFramebuffers(1, &ftex.fb);
	glNamedFramebufferTexture(ftex.fb, GL_COLOR_ATTACHMENT0, ftex.tex, 0);
	gl_check_err("Failed to attach texture to framebuffer");

	return ftex;
}

void gl_draw_frametex(gl_frametex ftex) {
	glBindFramebuffer(GL_READ_FRAMEBUFFER, ftex.fb);
	glBlitFramebuffer(
		0, 0, ftex.w, ftex.h,
		0, 0, ftex.w, ftex.h,
		GL_COLOR_BUFFER_BIT,
		GL_NEAREST
	);
}

GLuint gl_load_texture(const char *image_filename) {
	
	// Load file
	SDL_Surface *surf = SDL_LoadBMP(image_filename);
	if (surf == NULL) {
		__log_err("Failed to load texture image bitmap", image_filename);
		return 0;
	}

	// Convert Surface to known pixel format
	SDL_Surface *conv = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_ABGR8888, 0);
	if (conv == NULL) {
		__log_err("Failed to convert texture surface", SDL_GetError());
		return 0;
	}

	// Reverse Pixels for OpenGL
	SDL_FreeSurface(surf);
	surf = SDL_CreateRGBSurfaceWithFormat(
		conv->flags,
		conv->w, conv->h,
		conv->format->BitsPerPixel,
		SDL_PIXELFORMAT_ABGR8888
	);
	SDL_LockSurface(surf);
	Uint8 *source = (Uint8 *)(conv->pixels);
	Uint8 *target = (Uint8 *)(surf->pixels);
	int num_pixels = conv->w * conv->h;
	for (int i=0; i<num_pixels; i++) {
		int j = num_pixels - 1 - i;
		target[j*4+0] = source[i*4+0];
		target[j*4+1] = source[i*4+1];
		target[j*4+2] = source[i*4+2];
		target[j*4+3] = source[i*4+3];
	}
	SDL_UnlockSurface(surf);

	// Create texture and load data
	GLuint tex;
	glCreateTextures(GL_TEXTURE_2D, 1, &tex);

	glTextureStorage2D(tex, 1, GL_RGBA8, surf->w, surf->h);
	glTextureSubImage2D(tex, 0, 0, 0, surf->w, surf->h, GL_RGBA, GL_UNSIGNED_BYTE, surf->pixels);
	gl_check_err("Failed to upload texture");

	// Set parameters
	glTextureParameteri(tex, GL_TEXTURE_BASE_LEVEL, 0);
	glTextureParameteri(tex, GL_TEXTURE_MAX_LEVEL, 0);
	glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_REPEAT);
	gl_check_err("Failed to set texture params");

	// DEBUG: output first n pixels' data to check format
	int n = 16;
	printf("---> Converted Surface Format: \n");
	printf("      bits/px: %i\n", surf->format->BitsPerPixel);
	printf("      byte/px: %i\n", surf->format->BytesPerPixel);
	printf("       R-Mask: 0x%08X\n", surf->format->Rmask);
	printf("       G-Mask: 0x%08X\n", surf->format->Gmask);
	printf("       B-Mask: 0x%08X\n", surf->format->Bmask);
	printf("       A-Mask: 0x%08X\n", surf->format->Amask);
	printf("---> First %i pixels:", n);
	Uint8 *data = (Uint8 *) surf->pixels;
	for (int i=0; i<n; i++) {
		if ((i&0b11) == 0) printf("\n      ");
		printf("[ 0x%02X 0x%02X 0x%02X 0x%02X ]",
			data[i*4+0], data[i*4+1], data[i*4+2], data[i*4+3]
		);
	}
	putchar('\n'); fflush(stdout);

	SDL_FreeSurface(surf);
	return tex;
}

GLuint gl_load_debug_texture() {

	// Debug Texture image data
	GLfloat pixels[] = {
		0.0f, 1.0f, 1.0f,   1.0f, 0.0f, 1.0f,   1.0f, 1.0f, 0.0f,   1.0f, 1.0f, 1.0f,
		1.0f, 0.0f, 0.0f,   0.0f, 1.0f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f, 0.0f,
	};

	GLuint tex;
	glCreateTextures(GL_TEXTURE_2D, 1, &tex);
	glTextureStorage2D(tex, 1, GL_RGB32F, 4, 2);
	glTextureSubImage2D(tex, 0, 0, 0, 4, 2, GL_RGB, GL_FLOAT, pixels);
	gl_check_err("Failed to create debug texture");

	// Set parameters
	glTextureParameteri(tex, GL_TEXTURE_BASE_LEVEL, 0);
	glTextureParameteri(tex, GL_TEXTURE_MAX_LEVEL, 0);
	glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_REPEAT);
	gl_check_err("Failed to set debug texture params");

	//glGenerateTextureMipmap(tex);
	return tex;
}

void gl_check_err(char *msg) {
	__ensure_init();

	GLenum err = glGetError();
	char *gl_msg;
	switch (err) {
		case GL_NO_ERROR: return;
		case GL_INVALID_ENUM: gl_msg = "INVALID_ENUM: An unacceptable value is specified for an enumerated argument"; break;
		case GL_INVALID_VALUE: gl_msg = "INVALID_VALUE: A numeric argument is out of range"; break;
		case GL_INVALID_OPERATION: gl_msg = "INVALID_OPERATION: The specified operation is not allowed in the current state"; break;
		case GL_INVALID_FRAMEBUFFER_OPERATION: gl_msg = "The framebuffer object is not complete"; break;
		case GL_OUT_OF_MEMORY: gl_msg = "There is not enough memory left to execute the command"; break;
		case GL_STACK_UNDERFLOW: gl_msg = "An attempt has been made to perform an operation that would cause an internal stack to underflow"; break;
		case GL_STACK_OVERFLOW: gl_msg = "An attempt has been made to perform an operation that would cause an internal stack to overflow"; break;
	}
	__log_err(msg, gl_msg);
}

void gl_print_prog_log(GLuint program) {
	__ensure_init();

	// Make sure name is program
	if(!glIsProgram(program)) {
		__log_warn("Tried to print info log of non program", NULL);
		return;
	}

	int infoLogLength = 0;
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLength);
	if (infoLogLength <= 0) {
		__log_warn("Tried to print program info log, but info log length is zero.", NULL);
		return;
	}

	// Get and print info log
	char* infoLog = SDL_malloc(infoLogLength * sizeof(char));
	glGetProgramInfoLog(program, infoLogLength, NULL, infoLog);
	__log_info(infoLog, NULL);
	SDL_free(infoLog);
}

void gl_print_shader_log(GLuint shader) {
	__ensure_init();

	// Make sure name is program
	if(!glIsShader(shader)) {
		__log_warn("Tried to print info log of non-shader", NULL);
		return;
	}

	int infoLogLength = 0;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLength);
	if (infoLogLength <= 0) {
		__log_warn("Tried to print shader info log, but info log length is zero.", NULL);
		return;
	}

	// Get and print info log
	char* infoLog = SDL_malloc(infoLogLength * sizeof(char));
	glGetShaderInfoLog(shader, infoLogLength, NULL, infoLog);
	__log_info(infoLog, NULL);
	SDL_free(infoLog);
}