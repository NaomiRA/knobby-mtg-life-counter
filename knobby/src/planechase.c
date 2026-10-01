#include "planechase.h"
#include "game.h"
#include "net_sync.h"
#include "storage.h"
#include "ui_1p.h"
#include "ui_mp.h"
#include "esp_random.h"
#include "resources/planes_data.h"
#include "extra/others/imgfont/lv_imgfont.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#if !defined(SIMULATOR)
#include "esp_heap_caps.h"
#endif

#define PLANE_BACKGROUND_WIDTH 360
#define PLANE_BACKGROUND_HEIGHT 360

LV_IMG_DECLARE(planeswalker);
LV_IMG_DECLARE(chaos);

#define CHAOS_ICON_SIZE 16
#define CHAOS_SYMBOL_CODEPOINT 0xE61D

static uint8_t chaos_small_map[CHAOS_ICON_SIZE * CHAOS_ICON_SIZE / 2];
static const lv_img_dsc_t chaos_small = {
    .header.cf = LV_IMG_CF_ALPHA_4BIT,
    .header.w = CHAOS_ICON_SIZE,
    .header.h = CHAOS_ICON_SIZE,
    .data_size = sizeof(chaos_small_map),
    .data = chaos_small_map,
};
static lv_font_t plane_text_font;
static lv_font_t *plane_text_imgfont = NULL;

static uint8_t planeswalker_small_map[16 * 16 / 2];
static const lv_img_dsc_t planeswalker_small = {
    .header.cf = LV_IMG_CF_ALPHA_4BIT,
    .header.w = 16,
    .header.h = 16,
    .data_size = sizeof(planeswalker_small_map),
    .data = planeswalker_small_map,
};

lv_obj_t *screen_planechase = NULL;
bool planechase_active = false;

static int plane_deck[PLANE_CARD_COUNT];
static int plane_deck_count = 0;
static int plane_deck_position = 0;
static unsigned int plane_roll_counts[MAX_DISPLAY_PLAYERS];
static int plane_roll_player = 0;
static lv_obj_t *image_plane_background = NULL;
static lv_color_t *plane_background_pixels = NULL;
static lv_img_dsc_t plane_background_cached = {
    .header.cf = LV_IMG_CF_TRUE_COLOR,
    .header.w = PLANE_BACKGROUND_WIDTH,
    .header.h = PLANE_BACKGROUND_HEIGHT,
    .data_size = PLANE_BACKGROUND_WIDTH * PLANE_BACKGROUND_HEIGHT * sizeof(lv_color_t),
};
static lv_obj_t *label_plane_name = NULL;
static lv_obj_t *label_plane_viewer = NULL;
static lv_obj_t *label_plane_type = NULL;
static lv_obj_t *label_plane_text = NULL;
static lv_obj_t *label_plane_count = NULL;
static lv_obj_t *label_plane_result = NULL;
static lv_obj_t *label_plane_roll = NULL;
static lv_timer_t *plane_result_timer = NULL;
static char plane_image_path[32];

extern void back_to_main(void);

static uint8_t chaos_alpha_at(int x, int y)
{
    uint8_t pixel_pair = chaos.data[y * 16 + x / 2];
    return (x & 1) ? pixel_pair & 0x0f : pixel_pair >> 4;
}

static bool chaos_imgfont_path(const lv_font_t *font, void *img_src, uint16_t len,
                               uint32_t unicode, uint32_t unicode_next)
{
    (void)font;
    (void)unicode_next;
    if (unicode != CHAOS_SYMBOL_CODEPOINT || len < sizeof(chaos_small)) return false;
    memcpy(img_src, &chaos_small, sizeof(chaos_small));
    return true;
}

static void init_plane_text_font(void)
{
    if (plane_text_imgfont != NULL) return;

    for (int y = 0; y < CHAOS_ICON_SIZE; y++) {
        for (int x = 0; x < CHAOS_ICON_SIZE; x++) {
            uint8_t alpha = (chaos_alpha_at(x * 2, y * 2) +
                             chaos_alpha_at(x * 2 + 1, y * 2) +
                             chaos_alpha_at(x * 2, y * 2 + 1) +
                             chaos_alpha_at(x * 2 + 1, y * 2 + 1) + 2) / 4;
            int index = y * (CHAOS_ICON_SIZE / 2) + x / 2;
            chaos_small_map[index] |= (x & 1) ? alpha : alpha << 4;
        }
    }

    plane_text_imgfont = lv_imgfont_create(17, chaos_imgfont_path);
    if (plane_text_imgfont == NULL) return;
    plane_text_font = lv_font_mplantin_20;
    plane_text_font.fallback = plane_text_imgfont;
}

static bool cache_plane_background(int card_index)
{
    lv_img_decoder_dsc_t decoder;
    size_t pixel_buffer_size = sizeof(lv_color_t) * PLANE_BACKGROUND_WIDTH * PLANE_BACKGROUND_HEIGHT;
    bool decoded = true;

    snprintf(plane_image_path, sizeof(plane_image_path), "S:/planes/%03d.sjpg", card_index);

    if (plane_background_pixels == NULL) {
#if defined(SIMULATOR)
        plane_background_pixels = malloc(pixel_buffer_size);
#else
        plane_background_pixels = heap_caps_malloc(pixel_buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif
    }
    if (plane_background_pixels == NULL) return false;

    if (lv_img_decoder_open(&decoder, plane_image_path, lv_color_white(), 0) != LV_RES_OK) return false;
    if (decoder.header.w != PLANE_BACKGROUND_WIDTH || decoder.header.h != PLANE_BACKGROUND_HEIGHT) {
        lv_img_decoder_close(&decoder);
        return false;
    }

    for (int row_index = 0; row_index < PLANE_BACKGROUND_HEIGHT; row_index++) {
        uint8_t *row = (uint8_t *)(plane_background_pixels + row_index * PLANE_BACKGROUND_WIDTH);
        if (lv_img_decoder_read_line(&decoder, 0, row_index, PLANE_BACKGROUND_WIDTH, row) != LV_RES_OK) {
            decoded = false;
            break;
        }
    }
    lv_img_decoder_close(&decoder);
    if (!decoded) return false;

    plane_background_cached.data = (const uint8_t *)plane_background_pixels;
    lv_img_set_src(image_plane_background, &plane_background_cached);
    return true;
}

static void refresh_planechase_screen(void)
{
    int card_index = plane_deck[plane_deck_position];
    const plane_card_t *card = &plane_cards[card_index];
    char count[24];

    if (!cache_plane_background(card_index)) {
        snprintf(plane_image_path, sizeof(plane_image_path), "S:/planes/%03d.sjpg", card_index);
        lv_img_set_src(image_plane_background, plane_image_path);
    }
    lv_obj_center(image_plane_background);
    lv_label_set_text(label_plane_name, card->name);
    lv_label_set_text(label_plane_type, card->type_line);
    lv_label_set_text(label_plane_text, card->oracle_text);
    snprintf(count, sizeof(count), "%d / %d", plane_deck_position + 1, plane_deck_count);
    lv_label_set_text(label_plane_count, count);
    lv_obj_scroll_to_y(lv_obj_get_parent(label_plane_text), 0, LV_ANIM_OFF);
}

static void refresh_plane_roll_cost(void)
{
    char label[32];

    if (plane_roll_counts[plane_roll_player] == 0)
        lv_label_set_text(label_plane_roll, "Roll die\nFree");
    else {
        snprintf(label, sizeof(label), "Roll die\nCost %u", plane_roll_counts[plane_roll_player]);
        lv_label_set_text(label_plane_roll, label);
    }
}

unsigned int planechase_roll_cost(int player_index)
{
    if (!planechase_active || player_index < 0 || player_index >= MAX_DISPLAY_PLAYERS) return 0;
    return plane_roll_counts[player_index];
}

void create_plane_cost_row(lv_obj_t *parent, lv_obj_t **row, lv_obj_t **value)
{
    lv_obj_t *icon;
    static bool image_ready = false;

    if (!image_ready) {
        for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 16; x++) {
                uint8_t top = planeswalker.data[(y * 2) * 16 + x];
                uint8_t bottom = planeswalker.data[(y * 2 + 1) * 16 + x];
                uint8_t alpha = ((top >> 4) + (top & 0x0f) +
                                 (bottom >> 4) + (bottom & 0x0f) + 2) / 4;
                int index = y * 8 + x / 2;
                planeswalker_small_map[index] |= (x & 1) ? alpha : alpha << 4;
            }
        }
        image_ready = true;
    }

    *row = make_plain_box(parent, 34, 34);
    lv_obj_add_flag(*row, LV_OBJ_FLAG_HIDDEN);

    icon = lv_img_create(*row);
    lv_img_set_src(icon, &planeswalker_small);
    lv_obj_set_style_img_recolor(icon, lv_color_white(), 0);
    lv_obj_set_style_img_recolor_opa(icon, LV_OPA_COVER, 0);
    lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 0);

    *value = lv_label_create(*row);
    lv_obj_set_style_text_font(*value, &lv_font_beleren_bold_18, 0);
    lv_obj_align(*value, LV_ALIGN_BOTTOM_MID, 0, 0);
}

void planechase_pass_turn(int player_index)
{
    if (player_index < 0 || player_index >= MAX_DISPLAY_PLAYERS) return;
    plane_roll_counts[player_index] = 0;
    if (label_plane_roll != NULL && plane_roll_player == player_index) refresh_plane_roll_cost();
    refresh_player_ui();
}

static void set_plane_result_opa(void *obj, int32_t value)
{
    lv_obj_set_style_text_opa(obj, (lv_opa_t)value, 0);
}

static void clear_plane_result(void)
{
    lv_timer_pause(plane_result_timer);
    lv_anim_del(label_plane_result, set_plane_result_opa);
    lv_obj_set_style_text_opa(label_plane_result, LV_OPA_COVER, 0);
    lv_label_set_text(label_plane_result, "");
}

static void fade_plane_result(lv_timer_t *timer)
{
    lv_anim_t animation;

    lv_timer_pause(timer);
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, label_plane_result);
    lv_anim_set_exec_cb(&animation, set_plane_result_opa);
    lv_anim_set_values(&animation, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&animation, 500);
    lv_anim_start(&animation);
}

void open_planechase_screen(void)
{
    char viewer[80];
    int player;

    if (!planechase_active || plane_deck_count == 0) return;
    plane_roll_player = 0;
    if (lv_scr_act() != screen_multiplayer || selection_count() == 0) {
        snprintf(viewer, sizeof(viewer), "%s", player_names[0]);
    } else {
        for (player = 0; player < nvs_get_players_to_track(); player++) {
            if (is_player_selected(player)) break;
        }
        if (player < nvs_get_players_to_track()) plane_roll_player = player;
        snprintf(viewer, sizeof(viewer), "%s", player_names[plane_roll_player]);
    }
    lv_label_set_text(label_plane_viewer, viewer);
    refresh_planechase_screen();
    refresh_plane_roll_cost();
    clear_plane_result();
    lv_scr_load(screen_planechase);
    lv_indev_wait_release(lv_indev_get_act());
}

static void start_planechase_deck(int players)
{
    int i;

    memset(plane_roll_counts, 0, sizeof(plane_roll_counts));
    for (i = 0; i < PLANE_CARD_COUNT; i++) plane_deck[i] = i;
    for (i = PLANE_CARD_COUNT - 1; i > 0; i--) {
        int other = (int)(esp_random() % (unsigned)(i + 1));
        int card = plane_deck[i];
        plane_deck[i] = plane_deck[other];
        plane_deck[other] = card;
    }
    plane_deck_count = players * 10;
    if (plane_deck_count > PLANE_CARD_COUNT) plane_deck_count = PLANE_CARD_COUNT;
    plane_deck_position = 0;
}

void planechase_set_active(bool active, int players)
{
    planechase_active = active;
    if (planechase_active) start_planechase_deck(players);
    else plane_deck_count = 0;
}

static void event_plane_next(lv_event_t *e)
{
    (void)e;
    if (plane_deck_count > 1) {
        int next_position = (int)(esp_random() % (unsigned)(plane_deck_count - 1));
        if (next_position >= plane_deck_position) next_position++;
        plane_deck_position = next_position;
    }
    refresh_planechase_screen();
    clear_plane_result();
}

static void event_roll_planar_die(lv_event_t *e)
{
    int result = (int)(esp_random() % 6U);

    if (plane_roll_counts[plane_roll_player] < UINT_MAX)
        plane_roll_counts[plane_roll_player]++;
    refresh_plane_roll_cost();
    refresh_player_ui();
    clear_plane_result();
    if (result < 4) {
        lv_label_set_text(label_plane_result, "Nothing happens");
        lv_obj_set_style_text_color(label_plane_result, lv_color_hex(0xC0C0C0), 0);
    } else if (result == 4) {
        event_plane_next(e);
        lv_label_set_text(label_plane_result, "Planeswalk!");
        lv_obj_set_style_text_color(label_plane_result, lv_color_hex(0xA8D8BE), 0);
    } else {
        lv_label_set_text(label_plane_result, "Chaos ensues!");
        lv_obj_set_style_text_color(label_plane_result, lv_color_hex(0xFFCF70), 0);
    }
    lv_timer_reset(plane_result_timer);
    lv_timer_resume(plane_result_timer);
}

static void event_plane_close(lv_event_t *e)
{
    (void)e;
    clear_plane_result();
    back_to_main();
}

void build_planechase_screen(void)
{
    lv_obj_t *text_area;
    lv_obj_t *button;
    lv_obj_t *label;

    init_plane_text_font();

    screen_planechase = lv_obj_create(NULL);
    lv_obj_set_size(screen_planechase, 360, 360);
    lv_obj_set_style_bg_color(screen_planechase, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_planechase, 0, 0);
    lv_obj_set_scrollbar_mode(screen_planechase, LV_SCROLLBAR_MODE_OFF);

    image_plane_background = lv_img_create(screen_planechase);
    lv_img_set_src(image_plane_background, "S:/planes/000.sjpg");
    lv_obj_center(image_plane_background);
    lv_obj_set_style_img_opa(image_plane_background, LV_OPA_50, 0);

    // Close button
    button = lv_btn_create(screen_planechase);
    lv_obj_set_size(button, 55, 36);
    lv_obj_align(button, LV_ALIGN_TOP_MID, 0, 6);
    lv_obj_add_event_cb(button, event_plane_close, LV_EVENT_CLICKED, NULL);
    label = lv_label_create(button);
    lv_label_set_text(label, "Close");
    lv_obj_center(label);

    // Current player viewing plane
    label_plane_viewer = lv_label_create(screen_planechase);
    lv_obj_set_size(label_plane_viewer, 240, 18);
    lv_label_set_long_mode(label_plane_viewer, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(label_plane_viewer, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label_plane_viewer, lv_color_hex(0xA8D8BE), 0);
    lv_obj_set_style_text_font(label_plane_viewer, &lv_font_beleren_bold_18, 0);
    lv_obj_align(label_plane_viewer, LV_ALIGN_TOP_MID, 0, 50);

    // Plane name
    label_plane_name = lv_label_create(screen_planechase);
    lv_obj_set_width(label_plane_name, 260);
    lv_obj_set_style_text_align(label_plane_name, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label_plane_name, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_plane_name, &lv_font_beleren_bold_20, 0);
    lv_obj_align(label_plane_name, LV_ALIGN_TOP_MID, 0, 73);

    // Plane type
    label_plane_type = lv_label_create(screen_planechase);
    lv_obj_set_width(label_plane_type, 270);
    lv_obj_set_style_text_align(label_plane_type, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label_plane_type, lv_color_hex(0xA8D8BE), 0);
    lv_obj_set_style_text_font(label_plane_type, &lv_font_beleren_bold_18, 0);
    lv_obj_align(label_plane_type, LV_ALIGN_TOP_MID, 0, 94);

    // Plane description - scrollable text area
    text_area = lv_obj_create(screen_planechase);
    lv_obj_set_size(text_area, 270, 143);
    lv_obj_align(text_area, LV_ALIGN_TOP_MID, 0, 113);
    lv_obj_set_style_bg_opa(text_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(text_area, 0, 0);
    lv_obj_set_style_pad_all(text_area, 4, 0);
    lv_obj_set_scroll_dir(text_area, LV_DIR_VER);


    label_plane_text = lv_label_create(text_area);
    lv_obj_set_width(label_plane_text, 252);
    lv_obj_set_style_text_color(label_plane_text, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_plane_text,
                               plane_text_imgfont != NULL ? &plane_text_font : &lv_font_mplantin_20, 0);
    
    // Dice roll result
    label_plane_result = lv_label_create(screen_planechase);
    lv_label_set_text(label_plane_result, "");
    lv_obj_set_style_text_font(label_plane_result, &lv_font_beleren_bold_20, 0);
    lv_obj_align(label_plane_result, LV_ALIGN_TOP_MID, 0, 262);
    plane_result_timer = lv_timer_create(fade_plane_result, 5000, NULL);
    lv_timer_pause(plane_result_timer);

    // Roll planar die button
    button = lv_btn_create(screen_planechase);
    lv_obj_set_size(button, 90, 42);
    lv_obj_align(button, LV_ALIGN_BOTTOM_LEFT, 85, -30);
    lv_obj_add_event_cb(button, event_roll_planar_die, LV_EVENT_CLICKED, NULL);
    label_plane_roll = lv_label_create(button);
    lv_label_set_text(label_plane_roll, "Roll die\nFree");
    lv_obj_set_style_text_align(label_plane_roll, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(label_plane_roll);

    // Planeswalk button
    button = lv_btn_create(screen_planechase);
    lv_obj_set_size(button, 90, 42);
    lv_obj_align(button, LV_ALIGN_BOTTOM_RIGHT, -85, -30);
    lv_obj_add_event_cb(button, event_plane_next, LV_EVENT_CLICKED, NULL);
    // lv_obj_set_text_align(button, LV_TEXT_ALIGN_CENTER);
    label = lv_label_create(button);
    lv_label_set_text(label, "Planeswalk");
    lv_obj_center(label);

    // Plane count label (## / 40)
    label_plane_count = lv_label_create(screen_planechase);
    lv_obj_set_style_text_color(label_plane_count, lv_color_hex(0xA8D8BE), 0);
    lv_obj_align(label_plane_count, LV_ALIGN_BOTTOM_MID, 0, -10);
}