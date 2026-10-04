#include "bounty.h"
#include "game.h"
#include "timer.h"
#include "storage.h"
#include "ui_mp.h"
#include "esp_random.h"
#include "resources/bounties_data.h"
#include <stdlib.h>

#if !defined(SIMULATOR)
#include "esp_heap_caps.h"
#endif

bool bounty_active = false;
lv_obj_t *screen_bounty = NULL;

static int bounty_deck[BOUNTY_CARD_COUNT];
static int bounty_position;
static int bounty_tier = 1;
static bool bounty_claimed;
static int bounty_viewer = -1;
static bool bounty_multiplayer_view;
static lv_obj_t *image_bounty;
static lv_obj_t *label_bounty_viewer;
static lv_obj_t *label_bounty_name;
static lv_obj_t *label_bounty_text;
static lv_obj_t *label_bounty_status;
static lv_obj_t *label_bounty_count;
static lv_obj_t *button_bounty_claim;
static lv_obj_t *bounty_text_area;
static lv_obj_t *reward_overlay;
static lv_color_t *bounty_background_pixels;
static lv_img_dsc_t bounty_background = {
    .header.cf = LV_IMG_CF_TRUE_COLOR,
    .header.w = 360,
    .header.h = 360,
    .data_size = 360 * 360 * sizeof(lv_color_t),
};
static const char *const bounty_rewards[] = {
    "Create one Treasure token.",
    "Create two Treasure tokens.",
    "Create two Treasure tokens OR draw a card.",
    "Create two Treasure tokens AND draw a card.",
};

extern void back_to_main(void);

static void shuffle_bounties(void)
{
    int index;

    for (index = 0; index < BOUNTY_CARD_COUNT; index++) bounty_deck[index] = index;
    for (index = BOUNTY_CARD_COUNT - 1; index > 0; index--) {
        int other = (int)(esp_random() % (unsigned)(index + 1));
        int card = bounty_deck[index];
        bounty_deck[index] = bounty_deck[other];
        bounty_deck[other] = card;
    }
    bounty_position = 0;
}

void bounty_reset(void)
{
    if (reward_overlay != NULL) {
        lv_obj_del(reward_overlay);
        reward_overlay = NULL;
    }
    bounty_tier = 1;
    bounty_claimed = false;
    bounty_viewer = -1;
    if (bounty_active) shuffle_bounties();
}

void bounty_set_active(bool active)
{
    bounty_active = active;
    bounty_reset();
}

bool bounty_available(void)
{
    return bounty_active && turn_number >= 3;
}

void bounty_turn_changed(bool was_available)
{
    if (was_available && bounty_available() && !bounty_claimed && bounty_tier < 4)
        bounty_tier++;
    bounty_claimed = false;
}

int bounty_get_tier(void)
{
    return bounty_tier;
}

int bounty_get_card_index(void)
{
    return bounty_deck[bounty_position];
}

bool bounty_can_claim(int player)
{
    return bounty_available() && !bounty_claimed && player >= 0 &&
        player < MAX_DISPLAY_PLAYERS && player == turn_player_index &&
        !player_eliminated[player];
}

void bounty_next_card(void)
{
    if (!bounty_available()) return;
    bounty_position++;
    if (bounty_position >= BOUNTY_CARD_COUNT) shuffle_bounties();
    bounty_tier = 1;
}

bool bounty_claim_reward(int player)
{
    if (!bounty_can_claim(player)) return false;
    bounty_claimed = true;
    bounty_next_card();
    return true;
}

static bool cache_bounty_background(void)
{
    lv_img_decoder_dsc_t decoder;
    char path[40];
    bool decoded = true;
    size_t buffer_size = 360 * 360 * sizeof(lv_color_t);

    snprintf(path, sizeof(path), "S:/bounties/%03d.sjpg", bounty_get_card_index());
    if (bounty_background_pixels == NULL) {
#if defined(SIMULATOR)
        bounty_background_pixels = malloc(buffer_size);
#else
        bounty_background_pixels = heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif
    }
    if (bounty_background_pixels == NULL) return false;
    if (lv_img_decoder_open(&decoder, path, lv_color_white(), 0) != LV_RES_OK) return false;
    if (decoder.header.w != 360 || decoder.header.h != 360) {
        lv_img_decoder_close(&decoder);
        return false;
    }
    for (int row = 0; row < 360; row++) {
        if (lv_img_decoder_read_line(&decoder, 0, row, 360,
            (uint8_t *)(bounty_background_pixels + row * 360)) != LV_RES_OK) {
            decoded = false;
            break;
        }
    }
    lv_img_decoder_close(&decoder);
    if (!decoded) return false;
    bounty_background.data = (const uint8_t *)bounty_background_pixels;
    lv_img_set_src(image_bounty, &bounty_background);
    return true;
}

static bool viewer_can_claim(void)
{
    if (bounty_multiplayer_view &&
        (selection_count() != 1 || !is_player_selected(bounty_viewer))) return false;
    return bounty_can_claim(bounty_viewer);
}

static void refresh_bounty_screen(void)
{
    char text[800];
    const bounty_card_t *card = &bounty_cards[bounty_get_card_index()];

    if (cache_bounty_background()) lv_obj_clear_flag(image_bounty, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(image_bounty, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(label_bounty_name, card->name);

    snprintf(text, sizeof(text), "%s", card->oracle_text);
    lv_label_set_text(label_bounty_text, text);
    lv_obj_scroll_to_y(bounty_text_area, 0, LV_ANIM_OFF);
    snprintf(text, sizeof(text), "%d / %d", bounty_position + 1, BOUNTY_CARD_COUNT);
    lv_label_set_text(label_bounty_count, text);
    lv_label_set_text(label_bounty_status, bounty_claimed ? "Reward claimed this turn" :
        viewer_can_claim() ? "" : "Only active player may claim");
    lv_label_set_text_fmt(lv_obj_get_child(button_bounty_claim, 0), "Claim\nTier %d", bounty_tier);
    if (viewer_can_claim()) lv_obj_clear_state(button_bounty_claim, LV_STATE_DISABLED);
    else lv_obj_add_state(button_bounty_claim, LV_STATE_DISABLED);
}

void open_bounty_screen(void)
{
    if (!bounty_available()) return;
    if (reward_overlay != NULL) {
        lv_obj_del(reward_overlay);
        reward_overlay = NULL;
    }
    bounty_multiplayer_view = lv_scr_act() == screen_multiplayer;
    bounty_viewer = 0;
    if (bounty_multiplayer_view) {
        bounty_viewer = -1;
        if (selection_count() == 1) {
            for (int player = 0; player < nvs_get_players_to_track(); player++) {
                if (is_player_selected(player)) {
                    bounty_viewer = player;
                    break;
                }
            }
        }
    }
    lv_label_set_text(label_bounty_viewer, bounty_viewer >= 0 ?
        player_names[bounty_viewer] : "Shared bounty");
    refresh_bounty_screen();
    lv_scr_load(screen_bounty);
    lv_indev_wait_release(lv_indev_get_act());
}

static void event_bounty_close(lv_event_t *event)
{
    (void)event;
    back_to_main();
}

static void event_bounty_next(lv_event_t *event)
{
    (void)event;
    bounty_next_card();
    refresh_bounty_screen();
}

static void event_reward_dismiss(lv_event_t *event)
{
    (void)event;
    lv_obj_del(reward_overlay);
    reward_overlay = NULL;
}

static void event_bounty_claim(lv_event_t *event)
{
    int reward_tier = bounty_tier;
    lv_obj_t *message;
    lv_obj_t *label;
    lv_obj_t *button;

    (void)event;
    if (!viewer_can_claim() || !bounty_claim_reward(bounty_viewer)) return;
    refresh_bounty_screen();
    reward_overlay = lv_obj_create(screen_bounty);
    lv_obj_set_size(reward_overlay, 360, 360);
    lv_obj_set_style_bg_color(reward_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(reward_overlay, LV_OPA_80, 0);
    lv_obj_set_style_border_width(reward_overlay, 0, 0);
    lv_obj_set_style_pad_all(reward_overlay, 0, 0);
    lv_obj_clear_flag(reward_overlay, LV_OBJ_FLAG_SCROLLABLE);
    message = lv_obj_create(reward_overlay);
    lv_obj_set_size(message, 250, 190);
    lv_obj_center(message);
    lv_obj_set_style_radius(message, 6, 0);
    lv_obj_clear_flag(message, LV_OBJ_FLAG_SCROLLABLE);
    label = lv_label_create(message);
    lv_obj_set_width(label, 216);
    lv_label_set_text_fmt(label, "Reward claimed\n\n%s", bounty_rewards[reward_tier - 1]);
    lv_obj_set_style_text_font(label, &lv_font_beleren_bold_18, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);
    button = lv_btn_create(message);
    lv_obj_set_size(button, 80, 34);
    lv_obj_align(button, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(button, event_reward_dismiss, LV_EVENT_CLICKED, NULL);
    label = lv_label_create(button);
    lv_label_set_text(label, "OK");
    lv_obj_center(label);
}

static lv_obj_t *make_bounty_label(lv_obj_t *parent, int width, int top,
                                  const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_width(label, width);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, top);
    return label;
}

static lv_obj_t *make_bounty_button(const char *text, int width, int height,
                                   lv_align_t alignment, int horizontal, int vertical,
                                   lv_event_cb_t callback)
{
    lv_obj_t *button = lv_btn_create(screen_bounty);
    lv_obj_t *label = lv_label_create(button);

    lv_obj_set_size(button, width, height);
    lv_obj_set_style_radius(button, 6, 0);
    lv_obj_align(button, alignment, horizontal, vertical);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(label);
    return button;
}

void build_bounty_screen(void)
{
    screen_bounty = lv_obj_create(NULL);
    lv_obj_set_size(screen_bounty, 360, 360);
    lv_obj_set_style_bg_color(screen_bounty, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_bounty, 0, 0);
    lv_obj_set_style_pad_all(screen_bounty, 0, 0);
    lv_obj_clear_flag(screen_bounty, LV_OBJ_FLAG_SCROLLABLE);
    image_bounty = lv_img_create(screen_bounty);
    lv_obj_center(image_bounty);
    lv_obj_set_style_img_opa(image_bounty, LV_OPA_50, 0);
    lv_obj_add_flag(image_bounty, LV_OBJ_FLAG_HIDDEN);
    
    // Close button
    make_bounty_button("Close", 64, 32, LV_ALIGN_TOP_MID, 0, 16, event_bounty_close);

    // Player viewing bounty
    label_bounty_viewer = make_bounty_label(screen_bounty, 240, 54, &lv_font_beleren_bold_18, 0xA8D8BE);
    lv_label_set_long_mode(label_bounty_viewer, LV_LABEL_LONG_DOT);

    // Bounty name
    label_bounty_name = make_bounty_label(screen_bounty, 266, 77, &lv_font_beleren_bold_20, 0xFFFFFF);
    lv_obj_set_height(label_bounty_name, 25);
    lv_label_set_long_mode(label_bounty_name, LV_LABEL_LONG_SCROLL_CIRCULAR);
    
    // Bounty text area container
    bounty_text_area = lv_obj_create(screen_bounty);
    lv_obj_set_size(bounty_text_area, 270, 128);
    lv_obj_align(bounty_text_area, LV_ALIGN_TOP_MID, 0, 107);
    lv_obj_set_style_bg_opa(bounty_text_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bounty_text_area, 0, 0);
    lv_obj_set_style_pad_all(bounty_text_area, 4, 0);
    lv_obj_set_scroll_dir(bounty_text_area, LV_DIR_VER);

    // Bounty text area
    label_bounty_text = lv_label_create(bounty_text_area);
    lv_obj_set_width(label_bounty_text, 252);
    lv_obj_set_style_text_color(label_bounty_text, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_bounty_text, &lv_font_mplantin_20, 0);

    label_bounty_status = make_bounty_label(screen_bounty, 258, 249, &lv_font_beleren_bold_18, 0xFFCF70);

    // Claim reward button
    button_bounty_claim = make_bounty_button("Claim\nTier 1", 104, 40,
        LV_ALIGN_BOTTOM_MID, -56, -45, event_bounty_claim);

    // Next card button
    make_bounty_button("Next\nCard", 104, 40, LV_ALIGN_BOTTOM_MID, 56, -45, event_bounty_next);

    // Card in deck position (## / 12)
    label_bounty_count = make_bounty_label(screen_bounty, 150, 329, &lv_font_beleren_bold_18, 0xC0C0C0);
}