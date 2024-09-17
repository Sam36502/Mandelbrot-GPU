//	
//	Doom-Style "Demo" system for recording inputs/zoom-paths
//	
//	Helps make a consistent benchmark scenario
//	

#ifndef DEMO_H
#define DEMO_H


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#define DEMO_MAX_VARS 64
#define DEMO_cap 256 // Size of sequence memory chunks

#define DEMO_BIND(a) &a, sizeof(a)
#define DEMO_FILE_SIG "MBDF"


typedef enum {
	DEMO_VAR_NULL = 0x00,
	DEMO_VAR_SCREEN_X = 0x01,
	DEMO_VAR_SCREEN_Y = 0x02,
	DEMO_VAR_ZOOM = 0x03,
	DEMO_VAR_ITERS = 0x04,
} Demo_Var;

typedef enum {
	DEMO_INTEGER,
	DEMO_FLOAT,
} Demo_Type;

typedef Uint8 Demo_Value[8]; // Can store anything from 1 byte to a 64-bit int/double

typedef struct {
	void *ptr;
	Uint16 size;
	Uint16 type;
	float delta_per_ms; // Current change in value per ms for interpolation; Is 0.0f if no interpolation is being done
} Demo_Var_Binding;

typedef struct {
	Uint32 delta_time; // in milliseconds
	Demo_Var var;
	Demo_Value value;
} Demo_Keyframe;

typedef struct {
	Uint32 len;
	Uint32 cap;
	Demo_Keyframe *frames;
} Demo_Sequence;


extern bool demo_is_playing;


//	Set up demo system
//	
//void demo_setup();

//	Binds an alterable demo variable to a pointer
//	
//	When a demo is played back, this variable will
//	be automatically updated by 
void demo_bind_var(Demo_Var var, Demo_Type type, void *ptr, size_t size);

//	Creates a new, empty demo sequence
//	
//	Returns created demo or NULL on error.
//	Should be cleaned up with `demo_destroy_seq()`
Demo_Sequence *demo_create_seq();

//	Frees a demo sequence's memory
//	
void demo_destroy_seq(Demo_Sequence *seq);

//	Loads a demo sequence from a demo file
//	
//	Returns created demo or NULL on error.
//	Should be cleaned up with `demo_destroy_seq()`
Demo_Sequence *demo_load_seq(char *demo_filename);

//	Writes a demo sequence to a demo file
//	
//	Returns 0 on success, 1 otherwise.
int demo_store_seq(Demo_Sequence *seq, char *demo_filename);

//	Writes a (series of) keyframe(s) to a demo sequence
//	
//	Writes the delta time as time since last call to `demo_write_keyframe()`
//	If `count` is greater than 1, all keyframes after the first are written with
//	a delta time of zero so they are all applied at once.
void demo_write_keyframes(Demo_Sequence *seq, int count, Demo_Keyframe *keyframes);

//	Records the current values of the bound variables
//	
void demo_record_keyframe(Demo_Sequence *seq);

//	Updates the value of a sequence keyframe
//	
//	Any out of range values of `keyframe_id` will be silently ignored
//	If `new_delta_time` is negative, it will be left unaltered.
//	If `var_type` is 0x00 or `new_val` is NULL,	the vars will be left unchanged.
void demo_update_keyframe(Demo_Sequence *seq, Uint32 keyframe_id, Sint64 new_delta_time, Demo_Var var, Demo_Value new_value);

//	Initialises a keyframe struct with provided values
//	
Demo_Keyframe demo_create_keyframe(Demo_Var var, void *val, size_t val_size);

//	Prints out the contents of a sequence for debugging
//	
void demo_print_sequence(Demo_Sequence *seq);

//	Prints out the values of a single keyframe for debugging
//	
void demo_print_keyframe(Demo_Keyframe keyframe);

//	Plays a demo sequence
//	
//	Overrides any previously playing sequence
void demo_play(Demo_Sequence *sequence);

//	Stops the ongoing demo sequence playing
//	
void demo_stop();

//	Processes ticks for the demo system
//	
//	Should be called between event and draw in main loop
//	Automatically checks for elapsed ms since last call
//	Returns a bool indicating whether the frame needs to be redrawn
bool demo_tick();

//	Adds some float value to a Demo_Value respecting its var type
//	
void demo_value_add(Demo_Var var, Demo_Value val, float delta_val);

//	Gets the difference of two Demo_Values as a double, respecting their var type
//	
float demo_value_dif(Demo_Var var, Demo_Value start, Demo_Value target);

#endif