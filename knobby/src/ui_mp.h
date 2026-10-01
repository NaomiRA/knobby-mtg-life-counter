#ifndef _UI_MP_H
#define _UI_MP_H

#include "types.h"

// ---------- screens ----------
extern lv_obj_t *screen_multiplayer;

// ---------- functions ----------
void build_multiplayer_screen(void);
void rebuild_multiplayer_layout(int track);

void refresh_multiplayer_ui(void);
void refresh_multiplayer_timer_ui(void);
void multiplayer_set_game_mode_starting(bool starting);
bool mp_commander_damage_turn(int delta);
void mp_commander_damage_finish(void);
void mp_commander_damage_cancel(void);

int mp_player_seat_rotation(int player);

void select_kick_timer(void);

#endif // _UI_MP_H
