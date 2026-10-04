#ifndef KNOBBY_BOUNTY_H
#define KNOBBY_BOUNTY_H

#include "types.h"

extern bool bounty_active;
extern lv_obj_t *screen_bounty;

void bounty_set_active(bool active);
void bounty_reset(void);
bool bounty_available(void);
void bounty_turn_changed(bool was_available);
bool bounty_can_claim(int player);
bool bounty_claim_reward(int player);
void bounty_next_card(void);
int bounty_get_tier(void);
int bounty_get_card_index(void);
void open_bounty_screen(void);
void build_bounty_screen(void);

#endif