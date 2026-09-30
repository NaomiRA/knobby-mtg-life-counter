#ifndef _PLANECHASE_H
#define _PLANECHASE_H

#include "types.h"

extern lv_obj_t *screen_planechase;
extern bool planechase_active;

void planechase_set_active(bool active, int players);
void build_planechase_screen(void);
void open_planechase_screen(void);
void planechase_pass_turn(int player_index);
unsigned int planechase_roll_cost(int player_index);
void create_plane_cost_row(lv_obj_t *parent, lv_obj_t **row, lv_obj_t **value);

#endif // _PLANECHASE_H