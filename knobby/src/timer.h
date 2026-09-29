#ifndef _TIMER_H
#define _TIMER_H

#include "types.h"

// ---------- state ----------
extern bool turn_timer_enabled;
extern bool turn_indicator_visible;
extern bool turn_ui_visible;
extern uint32_t turn_elapsed_ms;
extern uint32_t turn_started_ms;
extern int turn_number;
extern int turn_player_index;

// ---------- functions ----------
void knob_timer_init(void);
void turn_timer_start_fresh_for_player(int player_index);
void turn_timer_reset(void);
uint32_t get_turn_elapsed_ms(void);
uint32_t get_game_elapsed_ms(void);
uint32_t get_timer_elapsed_ms(void);
void format_timer_elapsed(char *buf, uint32_t buffer_size);

// event callbacks used in screen builders
void event_turn_tap(lv_event_t *e);

#endif // _TIMER_H
