//
//     Grids for Max for Live.
//
// Port of Mutable Instruments Grids for Max4Live
//
// Author: Henri DAVID
// https://github.com/hdavids/Grids
//
// Based on code of Olivier Gillet (ol.gillet@gmail.com)
// https://github.com/pichenettes/eurorack/tree/master/grids
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//


#include "ext.h"
#include "resources.h"
#include <math.h>

// The standard random() function is not standard on Windows.
// We need to do this to setup the rand_s() function.
#ifdef WIN_VERSION
#define _CRT_RAND_S
#endif

typedef struct _grids
{
	t_object ob;
	t_atom   val;
	t_symbol *name;

	t_uint8 kNumParts;
	t_uint8 kStepsPerPattern;

	//parameters
	t_uint8 mode;
	t_uint8 map_x;
	t_uint8 map_y;
	t_uint8 randomness;
	t_uint8 euclidean_length[3];
	t_uint8 density[3];
	t_uint8 euclidean_step[3];

	//running vars
	t_uint8 part_perturbation[3];
	t_uint8 step;

	//output
	t_uint8 velocities[3];
	t_uint8 state;

	//outlets
	void    *outlet_kick_gate;
	void    *outlet_snare_gate;
	void    *outlet_hihat_gate;
	void    *outlet_kick_accent_gate;
	void    *outlet_snare_accent_gate;
	void    *outlet_hihat_accent_gate;

} t_grids;

static t_class *s_grids_class = NULL;

//max ojbect creation
void *grids_new(t_symbol *s, long argc, t_atom *argv);
void grids_free(t_grids *x);
void grids_assist(t_grids *x, void *b, long m, long a, char *s);

//max inlets
void grids_in_kick_density(t_grids *x, long n);
void grids_in_snare_density(t_grids *x, long n);
void grids_in_hihat_density(t_grids *x, long n);
void grids_in_map_x(t_grids *x, long n);
void grids_in_map_y(t_grids *x, long n);
void grids_in_randomness(t_grids *x, long n);
void grids_in_kick_euclidian_length(t_grids *x, long n);
void grids_in_snare_euclidian_length(t_grids *x, long n);
void grids_in_hihat_euclidian_length(t_grids *x, long n);
void grids_in_mode_and_clock(t_grids *x, long n);

//grids
void grids_run(t_grids *grids, long playHead);
void grids_reset(t_grids *grids);
void grids_evaluate(t_grids *grids);
void grids_evaluate_drums(t_grids *grids);
t_uint8 grids_read_drum_map(t_grids *grids, t_uint8 instrument);
void grids_evaluate_euclidean(t_grids *grids);
void grids_output(t_grids *grids);


int C74_EXPORT main(void)
{
	t_class *clazz;

	//create grids object
	clazz = class_new("grids", (method)grids_new, (method)grids_free, (long)sizeof(t_grids), 0L, A_GIMME, 0);

	//inlet definition
	class_addmethod(clazz, (method)grids_in_kick_density, "int", A_LONG, 0);
	class_addmethod(clazz, (method)grids_in_snare_density, "in1", A_LONG, 0);
	class_addmethod(clazz, (method)grids_in_hihat_density, "in2", A_LONG, 0);
	class_addmethod(clazz, (method)grids_in_map_x, "in3", A_LONG, 0);
	class_addmethod(clazz, (method)grids_in_map_y, "in4", A_LONG, 0);
	class_addmethod(clazz, (method)grids_in_randomness, "in5", A_LONG, 0);
	class_addmethod(clazz, (method)grids_in_kick_euclidian_length, "in6", A_LONG, 0);
	class_addmethod(clazz, (method)grids_in_snare_euclidian_length, "in7", A_LONG, 0);
	class_addmethod(clazz, (method)grids_in_hihat_euclidian_length, "in8", A_LONG, 0);
	class_addmethod(clazz, (method)grids_in_mode_and_clock, "in9", A_LONG, 0);

	//inlet tooltips
	class_addmethod(clazz, (method)grids_assist, "assist", A_CANT, 0);

	CLASS_ATTR_SYM(clazz, "name", 0, t_grids, name);

	class_register(CLASS_BOX, clazz);
	s_grids_class = clazz;

	return 0;
}


void *grids_new(t_symbol *s, long argc, t_atom *argv)
{

	t_grids *grids;

	if ((grids = (t_grids *)object_alloc(s_grids_class))) {

		//inlets
		for (t_uint8 i = 9; i > 0; i--)
			intin(grids, i);


		//outlets
		grids->outlet_hihat_accent_gate = intout((t_object *)grids);
		grids->outlet_snare_accent_gate = intout((t_object *)grids);
		grids->outlet_kick_accent_gate = intout((t_object *)grids);
		grids->outlet_hihat_gate = intout((t_object *)grids);
		grids->outlet_snare_gate = intout((t_object *)grids);
		grids->outlet_kick_gate = intout((t_object *)grids);

		//configuration
		grids->kNumParts = 3;
		grids->kStepsPerPattern = 32;

		//parameters
		grids->map_x = 64;
		grids->map_y = 64;
		grids->randomness = 10;
		grids->mode = 0;
		grids->euclidean_length[0] = 5;
		grids->euclidean_length[1] = 7;
		grids->euclidean_length[2] = 11;
		grids->density[0] = 32;
		grids->density[1] = 32;
		grids->density[2] = 32;

		//runing vars
		grids->part_perturbation[0] = 0;
		grids->part_perturbation[1] = 0;
		grids->part_perturbation[2] = 0;
		grids->euclidean_step[0] = 0;
		grids->euclidean_step[1] = 0;
		grids->euclidean_step[2] = 0;
		grids->step = 0;

		//output
		grids->state = 0;
		grids->velocities[0] = 0;
		grids->velocities[1] = 0;
		grids->velocities[2] = 0;

	}

	return grids;
}



void grids_assist(t_grids *x, void *b, long m, long a, char *s)
{
	if (m == ASSIST_INLET) {	
		// Inlets
		switch (a) {
		case 0: sprintf(s, "kick density"); break;
		case 1: sprintf(s, "snare density"); break;
		case 2: sprintf(s, "hihat density"); break;
		case 3: sprintf(s, "map X"); break;
		case 4: sprintf(s, "map Y"); break;
		case 5: sprintf(s, "randomness"); break;
		case 6: sprintf(s, "kick euclidian length"); break;
		case 7: sprintf(s, "snare euclidian length"); break;
		case 8: sprintf(s, "hihat euclidian length"); break;
		case 9: sprintf(s, "mode/clock (-1:drums, -2:euclidian, n>=0:clock)"); break;
		}
	} else {		
		// Outlets
		switch (a) {
		case 0: sprintf(s, "kick gate"); break;
		case 1: sprintf(s, "snare gate"); break;
		case 2: sprintf(s, "hihat gate"); break;
		case 3: sprintf(s, "kick accent gate"); break;
		case 4: sprintf(s, "snare accent gate"); break;
		case 5: sprintf(s, "hihat accent gate"); break;
		}
	}
}



void grids_free(t_grids *x)
{
}

// Max Inlets

void grids_in_kick_density(t_grids *grids, long n)
{
	if (n >= 0 && n <= 127) grids->density[0] = (t_uint8)n;
}

void grids_in_snare_density(t_grids *grids, long n)
{
	if (n >= 0 && n <= 127) grids->density[1] = (t_uint8)n;
}

void grids_in_hihat_density(t_grids *grids, long n)
{
	if (n >= 0 && n <= 127) grids->density[2] = (t_uint8)n;
}

void grids_in_map_x(t_grids *grids, long n)
{
	if (n >= 0 && n <= 127) grids->map_x = (t_uint8)(n % 256);
}
void grids_in_map_y(t_grids *grids, long n)
{
	if (n >= 0 && n <= 127) grids->map_y = (t_uint8)n;
}

void grids_in_randomness(t_grids *grids, long n)
{
	if (n >= 0 && n <= 127) grids->randomness = (t_uint8)n;
}

void grids_in_kick_euclidian_length(t_grids *grids, long n)
{
	if (n > 0 && n <= 32) grids->euclidean_length[0] = (t_uint8)(n);
}

void grids_in_snare_euclidian_length(t_grids *grids, long n)
{
	if (n > 0 && n <= 32) grids->euclidean_length[1] = (t_uint8)(n);
}

void grids_in_hihat_euclidian_length(t_grids *grids, long n)
{
	if (n > 0 && n <= 32) grids->euclidean_length[2] = (t_uint8)(n);
}


// Grids

void grids_in_mode_and_clock(t_grids *grids, long n)
{
	if (n >= 0) {
		grids_run(grids, n);
	} else {
		if (n == -1) {
			grids->mode = 0;
		}if (n == -2) {
			grids->mode = 1;
		}if (n == -3) {
			grids_reset(grids);
		}
	}
}


void grids_run(t_grids *grids, long playHead) {
	grids->step = playHead % 32;
	grids->state = 0;
	if (grids->mode == 1) {
		grids_evaluate_euclidean(grids);
	}
	else {
		grids_evaluate_drums(grids);
	}
	grids_output(grids);

	//increment euclidian clock.
	for (int i = 0; i < grids->kNumParts; i++)
		grids->euclidean_step[i] = (grids->euclidean_step[i] + 1) % grids->euclidean_length[i];
	
}

void grids_reset(t_grids *grids) {
	grids->euclidean_step[0] = 0;
	grids->euclidean_step[1] = 0;
	grids->euclidean_step[2] = 0;
	grids->step = 0;
	grids->state = 0;
	//object_post((t_object *)grids, "reset");
}


void grids_output(t_grids *grids) {
	if ((grids->state & 1) > 0)
		outlet_int(grids->outlet_kick_gate, grids->velocities[0]);
	if ((grids->state & 2) > 0)
		outlet_int(grids->outlet_snare_gate, grids->velocities[1]);
	if ((grids->state & 4) > 0)
		outlet_int(grids->outlet_hihat_gate, grids->velocities[2]);

	if ((grids->state & 8) > 0)
		outlet_int(grids->outlet_kick_accent_gate, grids->velocities[0]);
	if ((grids->state & 16) > 0) 
		outlet_int(grids->outlet_snare_accent_gate, grids->velocities[1]);
	if ((grids->state & 32) > 0)
		outlet_int(grids->outlet_hihat_accent_gate, grids->velocities[2]);
	
}

void grids_evaluate_drums(t_grids *grids) {
	// At the beginning of a pattern, decide on perturbation levels
	if (grids->step == 0) {
		for (int i = 0; i < grids->kNumParts; ++i) {
			t_uint8 randomness = grids->randomness >> 2;
#ifdef WIN_VERSION
			unsigned int rand;
			rand_s(&rand);
#else
			t_uint8 rand = random();
#endif
			t_uint8 rand2 = (t_uint8)(rand%256);
			grids->part_perturbation[i] = (rand2*randomness) >> 8;
		}
	}

	t_uint8 instrument_mask = 1;
	t_uint8 accent_bits = 0;
	for (int i = 0; i < grids->kNumParts; ++i) {
		t_uint8 level = grids_read_drum_map(grids, i);
		if (level < 255 - grids->part_perturbation[i]) {
			level += grids->part_perturbation[i];
		} else {
			// The sequencer from Anushri uses a weird clipping rule here. Comment
			// this line to reproduce its behavior.
			level = 255;
		}
		t_uint8 threshold = 255 - grids->density[i] * 2;
		if (level > threshold) {
			if (level > 192) {
				accent_bits |= instrument_mask;
			}
			grids->velocities[i] = level / 2;
			grids->state |= instrument_mask;
		}
		instrument_mask <<= 1;
	}
	grids->state |= accent_bits << 3;
}


t_uint8 grids_read_drum_map(t_grids *grids, t_uint8 instrument) {

	t_uint8 x = grids->map_x;
	t_uint8 y = grids->map_y;
	t_uint8 step = grids->step;

	int i = (int)floor(x*3.0 / 127);
	int j = (int)floor(y*3.0 / 127);
	t_uint8* a_map = drum_map[i][j];
	t_uint8* b_map = drum_map[i + 1][j];
	t_uint8* c_map = drum_map[i][j + 1];
	t_uint8* d_map = drum_map[i + 1][j + 1];

	int offset = (instrument * grids->kStepsPerPattern) + step;
	t_uint8 a = a_map[offset];
	t_uint8 b = b_map[offset];
	t_uint8 c = c_map[offset];
	t_uint8 d = d_map[offset];
	t_uint8 maxValue = 127;
	t_uint8 r = ( 
					  ( a * x	+ b * (maxValue - x) ) * y 
					+ ( c * x	+ d * (maxValue - x) ) * ( maxValue - y ) 
				) / maxValue / maxValue;
	return r;
}


void grids_evaluate_euclidean(t_grids *grids) {
	t_uint8 instrument_mask = 1;
	t_uint8 reset_bits = 0;
	// Refresh only on sixteenth notes.
	if (!(grids->step & 1)) {
		for (int i = 0; i < grids->kNumParts; ++i) {
			grids->velocities[i] = 100;
			t_uint8 density = grids->density[i] >> 2;
			t_uint16 address = (grids->euclidean_length[i] - 1) * 32 + density;
			t_uint32 step_mask = 1L << (t_uint32)grids->euclidean_step[i];
			t_uint32 pattern_bits = address < 1024 ? grids_res_euclidean[address] : 0;
			if (pattern_bits & step_mask) 
				grids->state |= instrument_mask;
			if (grids->euclidean_step[i] == 0)
				reset_bits |= instrument_mask;
			instrument_mask <<= 1;
		}
	}
	grids->state |= reset_bits << 3;
}

