#include "timer.h"
#include "storage.h"
#include "ui_mp.h"
#include "damage_log.h"

// Forward declaration
extern void refresh_turn_ui(void);
extern void back_to_main(void);

// ---------- state ----------
bool turn_timer_enabled = false;
bool turn_indicator_visible = true;
bool turn_ui_visible = false;
uint32_t turn_elapsed_ms = 0;
uint32_t turn_started_ms = 0;
int turn_number = 0;
int turn_player_index = 0;

static uint32_t game_elapsed_ms = 0;
static uint32_t game_started_ms = 0;
static uint8_t turn_blink_steps_remaining = 0;

static lv_timer_t *turn_timer = NULL;
static lv_timer_t *turn_blink_timer = NULL;

// ---------- functions ----------
uint32_t get_turn_elapsed_ms(void)
{
    uint32_t elapsed = turn_elapsed_ms;

    if (turn_timer_enabled) {
        elapsed += lv_tick_elaps(turn_started_ms);
    }

    return elapsed;
}

uint32_t get_game_elapsed_ms(void)
{
    uint32_t elapsed = game_elapsed_ms;

    if (turn_timer_enabled) {
        elapsed += lv_tick_elaps(game_started_ms);
    }

    return elapsed;
}

uint32_t get_timer_elapsed_ms(void)
{
    if (nvs_get_timer_mode() == TIMER_MODE_TOTAL) return get_game_elapsed_ms();
    if (nvs_get_timer_mode() == TIMER_MODE_TURN) return get_turn_elapsed_ms();
    return 0;
}

void format_timer_elapsed(char *buf, uint32_t buffer_size)
{
    uint32_t total_seconds;

    if (buf == NULL || buffer_size == 0) return;

    total_seconds = get_timer_elapsed_ms() / 1000;
    if (nvs_get_timer_mode() == TIMER_MODE_TURN) {
        snprintf(buf, buffer_size, "%lu:%02lu",
                 (unsigned long)(total_seconds / 60),
                 (unsigned long)(total_seconds % 60));
    } else {
        snprintf(buf, buffer_size, "%lu:%02lu",
                 (unsigned long)(total_seconds / 3600),
                 (unsigned long)((total_seconds % 3600) / 60));
    }
}

void turn_timer_start_fresh_for_player(int player_index)
{
    uint32_t now;
    int player_count = nvs_get_num_players();

    if (player_count < 1) player_count = 1;
    if (player_count > MAX_DISPLAY_PLAYERS) player_count = MAX_DISPLAY_PLAYERS;
    if (player_index < 0 || player_index >= player_count) player_index = 0;

    now = lv_tick_get();
    turn_number = 1;
    turn_player_index = player_index;
    turn_elapsed_ms = 0;
    turn_started_ms = now;
    game_elapsed_ms = 0;
    game_started_ms = now;
    turn_timer_enabled = true;
    turn_indicator_visible = true;
    turn_ui_visible = true;
    turn_blink_steps_remaining = 10;

    if (turn_blink_timer != NULL) {
        lv_timer_resume(turn_blink_timer);
    }

    refresh_turn_ui();
    refresh_multiplayer_timer_ui();
}

void turn_timer_reset(void)
{
    turn_timer_enabled = false;
    turn_elapsed_ms = 0;
    turn_started_ms = 0;
    turn_number = 0;
    turn_player_index = 0;
    game_elapsed_ms = 0;
    game_started_ms = 0;
    turn_indicator_visible = true;
    turn_ui_visible = false;
    turn_blink_steps_remaining = 0;

    if (turn_blink_timer != NULL) {
        lv_timer_pause(turn_blink_timer);
    }

    refresh_turn_ui();
    refresh_multiplayer_timer_ui();
}

// ---------- timer callbacks ----------
static void turn_timer_tick_cb(lv_timer_t *timer)
{
    (void)timer;
    refresh_turn_ui();
    refresh_multiplayer_timer_ui();
}

static void turn_blink_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (turn_blink_steps_remaining == 0) {
        turn_indicator_visible = true;
        if (turn_blink_timer != NULL) {
            lv_timer_pause(turn_blink_timer);
        }
        refresh_turn_ui();
        refresh_multiplayer_timer_ui();
        return;
    }

    turn_indicator_visible = !turn_indicator_visible;
    turn_blink_steps_remaining--;
    refresh_turn_ui();
    refresh_multiplayer_timer_ui();
}

// ---------- event callbacks ----------
void event_turn_tap(lv_event_t *e)
{
    int player_count;
    uint32_t now;

    (void)e;

    if (nvs_get_timer_mode() == TIMER_MODE_OFF) return;

    now = lv_tick_get();
    if (turn_number <= 0) {
        turn_number = 1;
        turn_player_index = 0;
        turn_elapsed_ms = 0;
        game_elapsed_ms = 0;
    } else {
        damage_log_add_turn(turn_player_index, get_turn_elapsed_ms());
        game_elapsed_ms = get_game_elapsed_ms();
        turn_elapsed_ms = 0;
        player_count = nvs_get_num_players();
        if (player_count < 1) player_count = 1;
        if (player_count > MULTIPLAYER_COUNT) player_count = MULTIPLAYER_COUNT;

        turn_player_index++;
        if (turn_player_index >= player_count) {
            turn_player_index = 0;
            turn_number++;
        }
    }

    turn_started_ms = now;
    game_started_ms = now;
    turn_timer_enabled = true;
    turn_ui_visible = true;
    refresh_turn_ui();
    refresh_multiplayer_timer_ui();
}

// ---------- init ----------
void knob_timer_init(void)
{
    turn_timer = lv_timer_create(turn_timer_tick_cb, 1000, NULL);
    if (turn_timer != NULL) {
        lv_timer_ready(turn_timer);
    }

    turn_blink_timer = lv_timer_create(turn_blink_timer_cb, 500, NULL);
    if (turn_blink_timer != NULL) {
        lv_timer_pause(turn_blink_timer);
    }
}
