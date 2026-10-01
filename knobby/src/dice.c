#include "dice.h"
#include "game.h"
#include "storage.h"
#include "ui_1p.h"
#include "esp_random.h"

#define DICE_MAX_ROLLS 10
#define DICE_MODE_COUNT 5

typedef enum {
    DICE_COIN,
    DICE_PLAYER,
    DICE_D6,
    DICE_D10,
    DICE_D20
} dice_mode_t;

lv_obj_t *screen_dice = NULL;
lv_obj_t *screen_dice_result = NULL;

static dice_mode_t dice_mode = DICE_D20;
static dice_mode_t result_mode = DICE_D20;
static int dice_count = 1;
static int result_count = 0;
static int result_values[DICE_MAX_ROLLS];
static int self_player = 0;
static bool include_self = true;
static lv_obj_t *mode_buttons[DICE_MODE_COUNT];
static lv_obj_t *self_button;
static lv_obj_t *self_label;
static lv_obj_t *include_button;
static lv_obj_t *include_label;
static lv_obj_t *count_label;
static lv_obj_t *empty_label;
static lv_obj_t *roll_button;
static lv_obj_t *result_title;
static lv_obj_t *result_total;
static lv_obj_t *result_labels[DICE_MAX_ROLLS];

static const char *mode_names[DICE_MODE_COUNT] = {
    "Coin Flip", "Random Player", "D6", "D10", "D20"
};

static int eligible_players(int *players)
{
    int player;
    int count = 0;

    for (player = 0; player < nvs_get_players_to_track(); player++) {
        if (player_eliminated[player] || (!include_self && player == self_player)) continue;
        players[count++] = player;
    }
    return count;
}

static void refresh_picker(void)
{
    char text[40];
    int players[MAX_DISPLAY_PLAYERS];
    int player_count = nvs_get_players_to_track();
    int mode;
    bool empty;

    if (screen_dice == NULL) return;
    if (self_player >= player_count) self_player = 0;

    for (mode = 0; mode < DICE_MODE_COUNT; mode++) {
        lv_obj_set_style_bg_color(mode_buttons[mode],
            lv_color_hex(mode == dice_mode ? 0x176D59 : 0x252831), 0);
        lv_obj_set_style_border_width(mode_buttons[mode], mode == dice_mode ? 2 : 0, 0);
    }

    if (dice_mode == DICE_PLAYER) {
        lv_obj_clear_flag(self_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(include_button, LV_OBJ_FLAG_HIDDEN);
        snprintf(text, sizeof(text), "You: %.16s", player_names[self_player]);
        lv_label_set_text(self_label, text);
        lv_label_set_text(include_label, include_self ? "Include yourself: Yes" : "Include yourself: No");
        lv_obj_center(self_label);
    } else {
        lv_obj_add_flag(self_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(include_button, LV_OBJ_FLAG_HIDDEN);
    }

    if(dice_count == 1) {
        snprintf(text, sizeof(text), "%s", mode_names[dice_mode]);
    } else {
        snprintf(text, sizeof(text), "%s x%d", mode_names[dice_mode], dice_count);
    }
    lv_label_set_text(count_label, text);
    lv_obj_align(count_label, LV_ALIGN_TOP_MID, 0, 259);
    empty = dice_mode == DICE_PLAYER && eligible_players(players) == 0;
    if (empty) {
        lv_obj_add_state(roll_button, LV_STATE_DISABLED);
        lv_obj_add_flag(count_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(empty_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_state(roll_button, LV_STATE_DISABLED);
        lv_obj_clear_flag(count_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(empty_label, LV_OBJ_FLAG_HIDDEN);
    }
}

void change_dice_count(int delta)
{
    dice_count += delta;
    if (dice_count < 1) dice_count = 1;
    if (dice_count > DICE_MAX_ROLLS) dice_count = DICE_MAX_ROLLS;
    refresh_picker();
}

static void refresh_result(void)
{
    char text[32];
    int total = 0;
    int index;

    if (screen_dice_result == NULL) return;
    if (result_count == 1) {
        snprintf(text, sizeof(text), "%s", mode_names[result_mode]);
    } else {
        snprintf(text, sizeof(text), "%s x%d", mode_names[result_mode], result_count);
    }
    lv_label_set_text(result_title, text);
    lv_obj_align(result_title, LV_ALIGN_TOP_MID, 0, 32);

    for (index = 0; index < DICE_MAX_ROLLS; index++) {
        if (index >= result_count) {
            lv_obj_add_flag(result_labels[index], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(result_labels[index], LV_OBJ_FLAG_HIDDEN);
        if (result_count == 1 && result_mode == DICE_PLAYER) {
            snprintf(text, sizeof(text), "%.16s", player_names[result_values[index]]);
        } else if (result_count == 1 && result_mode == DICE_COIN) {
            snprintf(text, sizeof(text), "%s", result_values[index] ? "Heads" : "Tails");
        } else if (result_count == 1) {
            total += result_values[index];
            snprintf(text, sizeof(text), "%d", result_values[index]);
        } else if (result_mode == DICE_PLAYER) {
            snprintf(text, sizeof(text), "%d. %.16s", index + 1, player_names[result_values[index]]);
        } else if (result_mode == DICE_COIN) {
            snprintf(text, sizeof(text), "%d. %s", index + 1,
                result_values[index] ? "Heads" : "Tails");
        } else {
            total += result_values[index];
            snprintf(text, sizeof(text), "%d. %d", index + 1, result_values[index]);
        }
        lv_label_set_text(result_labels[index], text);
        if (result_count == 1) {
            lv_obj_set_style_text_font(result_labels[index],
                result_mode >= DICE_D6 ? &lv_font_belerensmallcaps_bold_116 : &lv_font_beleren_bold_36, 0);
            lv_obj_set_width(result_labels[index], 270);
            lv_obj_set_style_text_align(result_labels[index], LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_align(result_labels[index], LV_ALIGN_CENTER, 0, 0);
        } else {
            lv_obj_set_style_text_font(result_labels[index],
                result_mode == DICE_PLAYER ? &lv_font_beleren_bold_18 : &lv_font_beleren_bold_26, 0);
            lv_obj_align(result_labels[index], LV_ALIGN_TOP_LEFT,
                80 + (index % 2) * 140, 100 + (index / 2) * 40);
            lv_obj_set_width(result_labels[index], 130);
            lv_obj_set_style_text_align(result_labels[index], LV_TEXT_ALIGN_LEFT, 0);
        }
    }

    if (result_mode >= DICE_D6 && result_count > 1) {
        snprintf(text, sizeof(text), "Total: %d", total);
        lv_label_set_text(result_total, text);
    } else {
        lv_label_set_text(result_total, "");
    }
    lv_obj_align(result_total, LV_ALIGN_TOP_MID, 0, 70);
}

void refresh_dice_ui(void)
{
    refresh_picker();
    if (result_count == 0 && dice_result > 0) {
        result_mode = DICE_D20;
        result_count = 1;
        result_values[0] = dice_result;
    }
    refresh_result();
}

#ifdef SIMULATOR
void dice_set_simulator_results(const int *values, int count)
{
    int index;

    if (count < 1 || count > DICE_MAX_ROLLS) return;
    result_mode = DICE_D20;
    result_count = count;
    for (index = 0; index < count; index++) result_values[index] = values[index];
    dice_result = values[0];
    refresh_result();
}
#endif

void open_dice_screen(void)
{
    refresh_picker();
    load_screen_if_needed(screen_dice);
}

void open_dice_result_screen(void)
{
    refresh_dice_ui();
    load_screen_if_needed(screen_dice_result);
}

static void event_dice_mode(lv_event_t *event)
{
    dice_mode = (dice_mode_t)(intptr_t)lv_event_get_user_data(event);
    dice_count = 1;
    refresh_picker();
}

static void event_dice_self(lv_event_t *event)
{
    (void)event;
    self_player = (self_player + 1) % nvs_get_players_to_track();
    refresh_picker();
}

static void event_dice_include(lv_event_t *event)
{
    (void)event;
    include_self = !include_self;
    refresh_picker();
}

static void event_dice_roll(lv_event_t *event)
{
    int players[MAX_DISPLAY_PLAYERS];
    int player_count = 0;
    int sides = 0;
    int index;
    (void)event;

    if (dice_mode == DICE_PLAYER) {
        player_count = eligible_players(players);
        if (player_count == 0) {
            refresh_picker();
            return;
        }
    }

    result_mode = dice_mode;
    result_count = dice_count;
    if (dice_mode == DICE_D6) sides = 6;
    if (dice_mode == DICE_D10) sides = 10;
    if (dice_mode == DICE_D20) sides = 20;

    for (index = 0; index < result_count; index++) {
        if (dice_mode == DICE_PLAYER)
            result_values[index] = players[esp_random() % (unsigned)player_count];
        else if (dice_mode == DICE_COIN)
            result_values[index] = (int)(esp_random() % 2U);
        else
            result_values[index] = (int)(esp_random() % (unsigned)sides) + 1;
    }
    dice_result = sides ? result_values[0] : 0;
    open_dice_result_screen();
}

void event_tool_dice(lv_event_t *event)
{
    (void)event;
    open_dice_screen();
}

static void event_dice_close(lv_event_t *event)
{
    (void)event;
    back_to_main();
}

static lv_obj_t *dice_button(lv_obj_t *parent, int x, int y, int width, int height,
                             const char *text, lv_event_cb_t callback, void *user_data)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_t *label = lv_label_create(button);

    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x252831), 0);
    lv_obj_set_style_border_color(button, lv_color_hex(0x06D6A0), 0);
    lv_obj_set_style_radius(button, 6, 0);
    lv_obj_set_style_pad_all(button, 0, 0);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, user_data);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, &lv_font_beleren_bold_18, 0);
    lv_obj_center(label);
    return button;
}

void build_dice_screen(void)
{
    lv_obj_t *label;
    int index;

    screen_dice = lv_obj_create(NULL);
    lv_obj_set_size(screen_dice, 360, 360);
    lv_obj_set_style_bg_color(screen_dice, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_dice, 0, 0);
    lv_obj_set_scrollbar_mode(screen_dice, LV_SCROLLBAR_MODE_OFF);

    label = lv_label_create(screen_dice);
    lv_label_set_text(label, "Dice");
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, &lv_font_beleren_bold_26, 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 30);

    for (index = 0; index < DICE_MODE_COUNT; index++) {
        int x = index == DICE_D20 ? 125 : (index % 2 ? 185 : 65);
        int y = 70 + (index / 2) * 42;
        mode_buttons[index] = dice_button(screen_dice, x, y, 110, 38,
            index == DICE_PLAYER ? "Random\nPlayer" : mode_names[index],
            event_dice_mode, (void *)(intptr_t)index);
    }

    self_button = dice_button(screen_dice, 73, 199, 214, 27, "", event_dice_self, NULL);
    self_label = lv_obj_get_child(self_button, 0);
    include_button = dice_button(screen_dice, 73, 229, 214, 27, "", event_dice_include, NULL);
    include_label = lv_obj_get_child(include_button, 0);

    count_label = lv_label_create(screen_dice);
    lv_obj_set_style_text_color(count_label, lv_color_hex(0x06D6A0), 0);
    lv_obj_set_style_text_font(count_label, &lv_font_beleren_bold_26, 0);

    empty_label = lv_label_create(screen_dice);
    lv_label_set_text(empty_label, "No eligible players");
    lv_obj_set_style_text_color(empty_label, lv_color_hex(0xF19B79), 0);
    lv_obj_set_style_text_font(empty_label, &lv_font_beleren_bold_18, 0);
    lv_obj_align(empty_label, LV_ALIGN_TOP_MID, 0, 265);

    roll_button = dice_button(screen_dice, 105, 293, 150, 43, "Roll", event_dice_roll, NULL);
    lv_obj_set_style_bg_color(roll_button, lv_color_hex(0x176D59), 0);

    screen_dice_result = lv_obj_create(NULL);
    lv_obj_set_size(screen_dice_result, 360, 360);
    lv_obj_set_style_bg_color(screen_dice_result, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_dice_result, 0, 0);
    lv_obj_set_scrollbar_mode(screen_dice_result, LV_SCROLLBAR_MODE_OFF);

    result_title = lv_label_create(screen_dice_result);
    lv_obj_set_style_text_color(result_title, lv_color_white(), 0);
    lv_obj_set_style_text_font(result_title, &lv_font_beleren_bold_26, 0);

    result_total = lv_label_create(screen_dice_result);
    lv_obj_set_style_text_color(result_total, lv_color_hex(0x06D6A0), 0);
    lv_obj_set_style_text_font(result_total, &lv_font_beleren_bold_26, 0);

    for (index = 0; index < DICE_MAX_ROLLS; index++) {
        result_labels[index] = lv_label_create(screen_dice_result);
        lv_label_set_long_mode(result_labels[index], LV_LABEL_LONG_DOT);
        lv_obj_set_pos(result_labels[index], 75, 110 + index * 21);
        lv_obj_set_width(result_labels[index], 210);
        lv_obj_set_style_text_color(result_labels[index], lv_color_white(), 0);
        lv_obj_set_style_text_font(result_labels[index], &lv_font_beleren_bold_18, 0);
    }
    dice_button(screen_dice_result, 105, 301, 150, 34, "Close", event_dice_close, NULL);
    refresh_dice_ui();
}