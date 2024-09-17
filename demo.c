#include "demo.h"

static Demo_Var_Binding __var_bindings[DEMO_MAX_VARS];
static Demo_Sequence *__curr_seq = NULL;

bool demo_is_playing = false;

// Returns whether value a == b
static bool __cmp_values(Demo_Value a, Demo_Value b) {
	for (int i=0; i<8; i++) {
		if (a[i] != b[i]) return false;
	}
	return true;
}


void demo_bind_var(Demo_Var var, Demo_Type type, void *ptr, size_t size) {
	if (ptr == NULL) return;
	if (size > 8) return;
	__var_bindings[var] = (Demo_Var_Binding){
		.ptr = ptr,
		.size = (Uint16) size,
		.type = (Uint16) type,
		.delta_per_ms = 0.0f,
	};
}

Demo_Sequence *demo_create_seq() {
	Demo_Sequence *seq = SDL_malloc(sizeof(Demo_Sequence));

	seq->cap = DEMO_cap;
	seq->frames = SDL_malloc(sizeof(Demo_Keyframe) * DEMO_cap);
	seq->len = 0;

	return seq;
}

void demo_destroy_seq(Demo_Sequence *seq) {
	SDL_free(seq->frames);
	SDL_free(seq);
}

Demo_Sequence *demo_load_seq(char *demo_filename) {
	if (demo_filename == NULL) return NULL;

	FILE *f = fopen(demo_filename, "rb");
	if (f == NULL) return NULL;

	// Read Signature
	char sig[5] = "XXXX\0";
	size_t rc = fread((Uint8 *)(sig), sizeof(Uint8), 4, f);
	if (rc == 0 || SDL_strcmp(DEMO_FILE_SIG, sig) != 0) {
		fclose(f);
		return NULL;
	}

	// Get Length
	fseek(f, 0L, SEEK_END);
	size_t size = ftell(f);
	fseek(f, 4L, SEEK_SET);

	Demo_Sequence *seq = SDL_malloc(sizeof(seq));
	seq->cap = size;
	seq->frames = SDL_malloc(size);
	seq->len = size / sizeof(Demo_Keyframe);

	// Write Keyframes
	rc = fread(seq->frames, sizeof(Demo_Keyframe), seq->len, f);
	if (rc != seq->len) {
		demo_destroy_seq(seq);
		fclose(f);
		return NULL;
	}

	fclose(f);
	return seq;
}

int demo_store_seq(Demo_Sequence *seq, char *demo_filename) {
	if (seq == NULL) return 1;
	if (demo_filename == NULL) return 1;

	FILE *f = fopen(demo_filename, "wb");

	// Write Signature
	size_t wc = fwrite((Uint8 *)(DEMO_FILE_SIG), sizeof(Uint8), 4, f);
	if (wc != 4) {
		fclose(f);
		return 1;
	}

	// Write Keyframes
	wc = fwrite(seq->frames, sizeof(Demo_Keyframe), seq->len, f);
	if (wc != seq->len) {
		fclose(f);
		return 1;
	}

	fclose(f);
	return 0;
}

void demo_write_keyframes(Demo_Sequence *seq, int count, Demo_Keyframe *keyframes) {
	static Uint64 ts_last_write = 0;
	Uint64 curr_ticks = SDL_GetTicks64();

	Uint64 delta_time = 0;
	if (ts_last_write > 0) delta_time = curr_ticks - ts_last_write;
	ts_last_write = curr_ticks;

	if (seq == NULL) return;
	if (count <= 0) return;
	if (keyframes == NULL) return;
	
	// Grow sequence if it's full
	if (seq->len+count >= seq->cap) {
		seq->cap += sizeof(Demo_Keyframe) * DEMO_cap;
		seq->frames = SDL_realloc(seq->frames, seq->cap);
	}

	keyframes[0].delta_time = delta_time;
	seq->frames[seq->len++] = keyframes[0];

	for (int i=1; i<count; i++) {
		keyframes[i].delta_time = 0;
		seq->frames[seq->len++] = keyframes[i];
	}
}

void demo_record_keyframe(Demo_Sequence *seq) {
	if (seq == NULL) return;
	Demo_Keyframe keyframes[DEMO_MAX_VARS];
	int bound_vars = 0;

	for (int i=0; i<DEMO_MAX_VARS; i++) {
		Demo_Var var = i;
		if (__var_bindings[var].size == 0) continue;

		Demo_Keyframe kf = demo_create_keyframe(var, __var_bindings[var].ptr, __var_bindings[var].size);

		// No keyframes have been recorded yet; skip repetition check
		if (seq->len == 0) {
			kf.delta_time = 0;
			keyframes[bound_vars++] = kf;
			continue;
		}

		// Check if value changed from last keyframe
		for (int j=seq->len-1; j>=0; j--) {
			Demo_Keyframe prev = seq->frames[j];
			if (prev.var != var) continue;
			if (__cmp_values(kf.value, prev.value)) break;

			keyframes[bound_vars++] = kf;
			break;
		}

	}

	demo_write_keyframes(seq, bound_vars, keyframes);
}

void demo_update_keyframe(Demo_Sequence *seq, Uint32 keyframe_id, Sint64 new_delta_time, Demo_Var var, Demo_Value new_value) {
	if (seq == NULL) return;
	if (keyframe_id >= seq->len) return;

	if (new_delta_time >= 0) {
		seq->frames[keyframe_id].delta_time = (Uint32) new_delta_time;
	}

	if (var != DEMO_VAR_NULL) {
		SDL_memcpy(seq->frames[keyframe_id].value, new_value, sizeof(Demo_Value));
	}
}

Demo_Keyframe demo_create_keyframe(Demo_Var var, void *val, size_t val_size) {
	Demo_Keyframe k;
	k.delta_time = 0;
	k.var = var;
	*(Uint64 *)(k.value) = 0L;

	if (val != NULL && val_size <= 8) {
		Uint8 *bytes = (Uint8 *) val;
		for (int i=0; i<val_size; i++) k.value[i] = bytes[i];
	}

	return k;
}

void demo_print_sequence(Demo_Sequence *seq) {
	puts("  Demo Sequence Debug Printout:");
	puts("---------------------------------");
	if (seq == NULL) {
		puts("  Sequence is NULL!");
		return;
	}

	for (Uint32 i=0; i<seq->len; i++) {
		printf("  [%04u] ", i);
		demo_print_keyframe(seq->frames[i]);
	}
}

void demo_print_keyframe(Demo_Keyframe keyframe) {
	float seconds = (float)(keyframe.delta_time) / 1000;
	int minutes = seconds / 60.0f;
	seconds -= minutes * 60;
	printf("Δt = %6u ms (%02i:%04.4f): ", keyframe.delta_time, minutes, seconds);
	switch (keyframe.var) {
		case DEMO_VAR_NULL:		printf("    (NULL): "); break;
		case DEMO_VAR_SCREEN_X:	printf("  Screen X: "); break;
		case DEMO_VAR_SCREEN_Y:	printf("  Screen Y: "); break;
		case DEMO_VAR_ZOOM:		printf("      Zoom: "); break;
		case DEMO_VAR_ITERS:	printf("Iterations: "); break;
	}
	int size = __var_bindings[keyframe.var].size;
	switch (size) {
		case 0: printf("Unbound!"); break;
		case 1: {
			Uint8 v = keyframe.value[0];
			printf("[8b] 0x%02X (%c)", v, v);
		} break;
		case 2: {
			Uint16 v = *(Uint16 *)(&keyframe.value[0]);
			printf("[16b] 0x%04X", v);
		} break;
		case 4: {
			int iv = *(int *)(&keyframe.value[0]);
			unsigned int uv = *(unsigned int *)(&keyframe.value[0]);
			float fv = *(float *)(&keyframe.value[0]);
			printf("[32b] %i (%u), %f", iv, uv, fv);
		} break;
		case 8: {
			long long int iv = *(long long int *)(&keyframe.value[0]);
			long long unsigned int uv = *(long long unsigned int *)(&keyframe.value[0]);
			double fv = *(double *)(&keyframe.value[0]);
			printf("[64b] %lli (%llu), %lf", iv, uv, fv);
		} break;
		default: {
			printf("[%ib] 0x", size*8);
			for (int i=0; i<size; i++) printf("%02X ", keyframe.value[i]);
		} break;
	}
	putchar('\n');
	fflush(stdout);
}

void demo_play(Demo_Sequence *seq) {
	if (seq == NULL) return;

	__curr_seq = seq;
	demo_is_playing = true;
}

void demo_stop() {
	__curr_seq = NULL;
	demo_is_playing = false;
}

bool demo_tick() {
	static Uint64 ts_last_tick = 0;
	static Uint32 curr_delta_time = 0;
	static Uint32 next_keyframe_index = 0;

	Uint64 curr_ticks = SDL_GetTicks64();
	if (ts_last_tick == 0) ts_last_tick = curr_ticks;
	Uint64 elapsed_ms = curr_ticks - ts_last_tick; // ms since previous call to demo_tick
	ts_last_tick = curr_ticks;

	if (__curr_seq == NULL) return false;
	if (!demo_is_playing) return false;

	bool redraw = false;

	// Update all variables being interpolated until delta time has passed
	if (elapsed_ms < curr_delta_time) {
		for (int v=0; v<DEMO_MAX_VARS; v++) {
			Demo_Var var = v;
			Demo_Var_Binding bind = __var_bindings[var];
			if (bind.size == 0) continue;
			if (bind.delta_per_ms == 0.0f) continue;

			Demo_Keyframe kf = demo_create_keyframe(var, bind.ptr, bind.size);
			demo_value_add(var, kf.value, bind.delta_per_ms * elapsed_ms);
			Uint8 *ptr_cast = (Uint8 *) bind.ptr;
			for (int i=0; i<bind.size; i++) ptr_cast[i] = kf.value[i];

			redraw = true;
		}
	
		curr_delta_time -= elapsed_ms;
		return redraw;
	} else {
		curr_delta_time = 0;
	}

	// Handle finishing a full keyframe period
	printf("---> Processing keyframes at [%04i]\n", next_keyframe_index);
	for (int frame_index=next_keyframe_index; frame_index<__curr_seq->len; frame_index++) {
		Demo_Keyframe keyf = __curr_seq->frames[frame_index];

		// Check if keyframe (not the first) has a new delta t
		if (frame_index > next_keyframe_index && keyf.delta_time > 0) {
			curr_delta_time = keyf.delta_time;
			next_keyframe_index = frame_index;
			break;
		}

		// Set variable to final keyframe value
		Demo_Var_Binding bind = __var_bindings[keyf.var];
		Uint8 *bytes = (Uint8 *) bind.ptr;
		for (int i=0; i<bind.size; i++) bytes[i] = keyf.value[i];
		redraw = true;

		// Unset delta to stop interpolation
		__var_bindings[keyf.var].delta_per_ms = 0.0f;

		// Check if we've reached the end
		if (frame_index >= __curr_seq->len - 1) {
			puts("---> Reached end of Demo; Stopping...");
			curr_delta_time = 0;
			next_keyframe_index = 0;
			demo_stop();
			return redraw; // always true
		}
	}

	// Scan next chunk of keyframes for interpolation values
	for (int frame_index=next_keyframe_index; frame_index<__curr_seq->len; frame_index++) {
		Demo_Keyframe kf_target = __curr_seq->frames[frame_index];
		if (frame_index > next_keyframe_index && kf_target.delta_time > 0) break;

		Demo_Var_Binding bind = __var_bindings[kf_target.var];
		Demo_Keyframe kf_curr = demo_create_keyframe(kf_target.var, bind.ptr, bind.size);
		
		// Check if values change from now until next keyframe
		if (__cmp_values(kf_curr.value, kf_target.value)) continue;

		// Calculate and set new delta
		float diff = demo_value_dif(kf_target.var, kf_curr.value, kf_target.value);
		__var_bindings[kf_target.var].delta_per_ms = diff / curr_delta_time;
	}

	// DEBUG
	fflush(stdout);
	return redraw;
}

void demo_value_add(Demo_Var var, Demo_Value val, float delta_val) {
	Demo_Var_Binding bind = __var_bindings[var];

	switch ((Demo_Type) bind.type) {

		case DEMO_INTEGER: {
			Uint64 i_val;
			for (int i=0; i<8; i++) *((Uint8 *)(&i_val) + i) = val[i];
			Uint64 result = (float)(i_val) + delta_val;
			for (int i=0; i<8; i++) val[i] = *((Uint8 *)(&result)+i);
			return;
		} break;

		case DEMO_FLOAT: {
			if (bind.size == 4) {
				float f_val;
				for (int i=0; i<4; i++) *((Uint8 *)(&f_val) + i) = val[i];
				float result = f_val + delta_val;
				for (int i=0; i<4; i++) val[i] = *((Uint8 *)(&result)+i);
				return;
			}
			if (bind.size == 8) {
				double f_val;
				for (int i=0; i<8; i++) *((Uint8 *)(&f_val) + i) = val[i];
				double result = f_val + delta_val;

				// Hacky fix to counter non-linear zooming
				// TODO: log?
				//if (var == DEMO_VAR_ZOOM) result = f_val * delta_val;

				for (int i=0; i<8; i++) val[i] = *((Uint8 *)(&result)+i);
				return;
			}
		} break;

	}
}

float demo_value_dif(Demo_Var var, Demo_Value start, Demo_Value target) {
	Demo_Var_Binding bind = __var_bindings[var];
	switch ((Demo_Type) bind.type) {

		case DEMO_INTEGER: {
			Uint64 i_start, i_target;
			for (int i=0; i<8; i++) *((Uint8 *)(&i_start) + i) = start[i];
			for (int i=0; i<8; i++) *((Uint8 *)(&i_target) + i) = target[i];
			return (float)(i_target) - (float)(i_start);
		} break;

		case DEMO_FLOAT: {
			if (bind.size == 4) {
				float f_start, f_target;
				for (int i=0; i<4; i++) *((Uint8 *)(&f_start) + i) = start[i];
				for (int i=0; i<4; i++) *((Uint8 *)(&f_target) + i) = target[i];
				return f_target - f_start;
			}
			if (bind.size == 8) {
				double f_start, f_target;
				for (int i=0; i<8; i++) *((Uint8 *)(&f_start) + i) = start[i];
				for (int i=0; i<8; i++) *((Uint8 *)(&f_target) + i) = target[i];

				// Hacky fix to counter non-linear zooming
				// TODO: log?
				//if (var == DEMO_VAR_ZOOM) return (float)(f_target / f_start);

				return (float)(f_target - f_start);
			}
		} break;

	}

	return 0.0f;
}