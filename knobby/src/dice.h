#ifndef _DICE_H
#define _DICE_H

#include "types.h"

// ---------- state ----------
extern lv_obj_t *screen_dice;
extern lv_obj_t *screen_dice_result;

// ---------- functions ----------
void build_dice_screen(void);
void refresh_dice_ui(void);
void open_dice_screen(void);
void open_dice_result_screen(void);
void change_dice_count(int delta);
#ifdef SIMULATOR
void dice_set_simulator_results(const int *values, int count);
#endif

// event callback used in menu builder
void event_tool_dice(lv_event_t *e);

#endif // _DICE_H
