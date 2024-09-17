#include <SDL2/SDL.h>
#include <SDL2/SDL_timer.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "gl.h"
#include "demo.h"


#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 1024

#define INPUT_MOUSE	0b00000001
#define INPUT_SHIFT	0b00000010

#define ZOOM_FACTOR 1.1f
#define ZOOM_FACTOR_FAST 1.5f
#define TIME_INTERVAL 50 // ms
#define THRESHOLD 2.0f

#define DEMO_FILENAME "demo_file.bin"
#define FPS_AVG_RANGE 10 // How many frames to average over for the framerate


void err_msg(const char *msg);

static SDL_Window *g_window = NULL;

int main(int argc, char* args[]) {

	// Initialisation
	if (SDL_Init(SDL_INIT_VIDEO) < 0) err_msg("Failed to initialise SDL");
	g_window = SDL_CreateWindow(
		"Mandelbrot as a Compute Shader",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		SCREEN_WIDTH, SCREEN_HEIGHT,
		SDL_WINDOW_OPENGL
		| SDL_WINDOW_SHOWN
	);
	if (g_window == NULL) err_msg("Failed to create Window");

	// Initialise OpenGL version 4.5
	gl_init(4, 5, g_window);

	// Create Program
	GLuint program = glCreateProgram();
	GLuint comp_shader = gl_load_shader(GL_COMPUTE_SHADER, "shaders/mandelbrot_float.comp");
	glAttachShader(program, comp_shader);
	gl_link_program(program);

	// Create Framebuffer/Texture
	gl_frametex frametex = gl_create_frametex(SCREEN_WIDTH, SCREEN_HEIGHT);
	glBindImageTexture(0, frametex.tex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);

	// Set up view window
	double screen_x = -1.0f;
	double screen_y = -1.0f;
	double zoom = 32.0f;
	GLuint iterations = 200;

	// Set up demo recording/playback
	demo_bind_var(DEMO_VAR_SCREEN_X, DEMO_FLOAT, DEMO_BIND(screen_x));
	demo_bind_var(DEMO_VAR_SCREEN_Y, DEMO_FLOAT, DEMO_BIND(screen_y));
	demo_bind_var(DEMO_VAR_ZOOM, DEMO_FLOAT, DEMO_BIND(zoom));
	demo_bind_var(DEMO_VAR_ITERS, DEMO_INTEGER, DEMO_BIND(iterations));
	Demo_Sequence *rec_demo = NULL;

	// Main Loop
	bool isRunning = true;
	bool redraw = true;
	SDL_Event curr_event;
	Uint8 input_mask = 0b00000000;
	double avg_fps = 0.0f;

	while (isRunning) {

		// Handle events
		int scode = SDL_WaitEventTimeout(&curr_event, 10);

		// Clock the demo system
		redraw |= demo_tick();

		if (scode != 0) {
			switch (curr_event.type) {
				case SDL_QUIT:
					isRunning = false;
				continue;

				case SDL_KEYDOWN: {
					SDL_KeyCode kc = curr_event.key.keysym.sym;
					switch (kc) {
						case SDLK_LSHIFT:
						case SDLK_RSHIFT: input_mask |= INPUT_SHIFT; break;
						case SDLK_KP_PLUS: {
							if (iterations < 1024) iterations++;
							printf("Nr. of Iterations: %i\n", iterations);
						} break;
						case SDLK_KP_MINUS: {
							if (iterations > 0) iterations--;
							printf("Nr. of Iterations: %i\n", iterations);
						} break;
						default: break;
					}
					fflush(stdout);
					redraw = true;
				} break;

				case SDL_KEYUP: {
					SDL_KeyCode kc = curr_event.key.keysym.sym;
					switch (kc) {
						case SDLK_ESCAPE: isRunning = false; continue;
						case SDLK_LSHIFT:
						case SDLK_RSHIFT: input_mask &= ~INPUT_SHIFT; break;
						case SDLK_F5: printf("---> Average FPS over last %i frames: %4.4lf\n", FPS_AVG_RANGE, avg_fps); break;

						// Start recording demo (deletes previous if present)
						case SDLK_F9: {
							// Delete and make new recording if shift held
							if (input_mask & INPUT_SHIFT) {
								demo_destroy_seq(rec_demo);
								rec_demo = NULL;
								puts("---> Deleted recorded demo sequence");
							}

							if (rec_demo == NULL) {
								rec_demo = demo_create_seq();
								demo_record_keyframe(rec_demo);
								puts("---> Started Recording Demo!");
							} else {
								demo_record_keyframe(rec_demo);
								puts("---> Finished Recording Demo!");
								printf("---> Captured %i Keyframes\n", rec_demo->len);
								demo_print_sequence(rec_demo);
							}
						} break;
						case SDLK_F10: {
							if (rec_demo == NULL) break;
							demo_record_keyframe(rec_demo);
							puts("---> Captured Keyframe");
						} break;
						case SDLK_F11: {
							if (rec_demo == NULL) {
								puts("---> No current recorded demo to play back!");
								break;
							}
							if (demo_is_playing) {
								puts("---> Stopped demo playback");
								demo_stop();
							} else {
								puts("---> Playing Demo...");
								demo_play(rec_demo);
							}
						} break;
						case SDLK_SPACE: {
							demo_is_playing = !demo_is_playing;
						} break;

						// Read Write Demo Files
						case SDLK_F1: {
							if (rec_demo != NULL) {
								if (demo_is_playing) demo_stop();
								demo_destroy_seq(rec_demo);
							}
							printf("---> Read Demo from '%s'\n", DEMO_FILENAME);
							rec_demo = demo_load_seq(DEMO_FILENAME);
						} break;
						case SDLK_F2: {
							printf("---> Writing Demo to '%s'...\n", DEMO_FILENAME);
							int err = demo_store_seq(rec_demo, DEMO_FILENAME);
							if (err != 0) printf("---> Failed!\n");
						} break;

						default: break;
					};
					fflush(stdout);
					redraw = true;
				} break;

				case SDL_MOUSEBUTTONDOWN: input_mask |= INPUT_MOUSE; break;
				case SDL_MOUSEBUTTONUP: input_mask &= ~INPUT_MOUSE; break;

				case SDL_MOUSEMOTION: {
					if ((input_mask & INPUT_MOUSE) == 0) break;

					double rel_x = (1/zoom) * curr_event.motion.xrel;
					double rel_y = -(1/zoom) * curr_event.motion.yrel;
					screen_x -= rel_x;
					screen_y -= rel_y;
					redraw = true;
				} break;

				case SDL_MOUSEWHEEL: {
					double factor = ZOOM_FACTOR;
					if (input_mask & INPUT_SHIFT) factor = ZOOM_FACTOR_FAST;

					if (curr_event.wheel.y < 0) {
						zoom *= 1/factor;
					} else {
						zoom *= factor;
					}

					if (zoom < 1) zoom = 1;
					redraw = true;
				} break;
			}
		}

		if (redraw) {
			// Clear Screen
			glClear(GL_COLOR_BUFFER_BIT);
			gl_check_err("Failed to clear colour buffer");

			// Start rendering
			Uint64 start = SDL_GetTicks64();
			glUseProgram(program);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, frametex.tex);
			gl_check_err("Failed draw setup");

			glUniform3f(0, (float) screen_x, (float) screen_y, (float) zoom);
			glUniform1ui(1, iterations);
			gl_check_err("Failed uniform");
			glDispatchCompute(SCREEN_WIDTH, SCREEN_HEIGHT, 1);
			glMemoryBarrier(GL_ALL_BARRIER_BITS);

			gl_draw_frametex(frametex);
			gl_check_err("Failed to call compute shader");

			// Done
			glUseProgram(NULL_PROGRAM);
			SDL_GL_SwapWindow(g_window);
			
			// Get Render time and add to running average
			Uint64 end = SDL_GetTicks64();
			if (end > start) {
				double time = end - start;
				double fps = 1000.0f / time;
				avg_fps -= avg_fps / FPS_AVG_RANGE;
				avg_fps += fps / FPS_AVG_RANGE;
			}

			redraw = false;
		}

	}

	// Termination
	gl_term();
	SDL_DestroyWindow(g_window);
	SDL_Quit();
	return 0;
}

void err_msg(const char *msg) {
	printf("[ERROR] %s: %s\n", msg, SDL_GetError());
	exit(1);
}
