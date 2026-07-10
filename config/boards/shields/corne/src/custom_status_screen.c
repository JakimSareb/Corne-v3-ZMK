/*
 * Custom Corne peripheral (right half) display:
 * pixel cat + active layer (synced from central) + battery/BT widgets.
 *
 * The image declaration mirrors ZMK's in-tree nice_view art (LVGL I1
 * format: 8-byte palette followed by rows padded to whole bytes), and the
 * cat is only ever swapped between two const images — no runtime pixel
 * mutation, so nothing here depends on LVGL decoder internals.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/display/widgets/battery_status.h>
#include <zmk/display/widgets/peripheral_status.h>
#include <lvgl.h>

#if IS_ENABLED(CONFIG_CORNE_LAYER_SYNC)
uint8_t corne_synced_layer(void); /* from behavior_layer_fwd.c */
#else
static inline uint8_t corne_synced_layer(void) { return 0; }
#endif

/* Keep in sync with the display-name properties in config/corne.keymap.
 * Only the layer index reaches the peripheral, so names live here. */
static const char *const layer_names[] = {"BASE", "SYM", "NAV"};

/* ── Cat, 28x20, two frames (eyes open / blink) ── */

#define CAT_W 28
#define CAT_H 20

static const uint8_t cat_open_map[] = {
    0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0c, 0x03, 0x00, 0x00,
    0x0e, 0x07, 0x00, 0x00, 0x0b, 0x0d, 0x00, 0x00, 0x09, 0xf9, 0x00, 0x00,
    0x10, 0x00, 0x80, 0x00, 0x10, 0x00, 0x80, 0x00, 0x20, 0x00, 0x40, 0x00,
    0x26, 0x06, 0x40, 0x00, 0x26, 0x06, 0x40, 0x00, 0x20, 0x00, 0x40, 0x00,
    0xa0, 0x60, 0x50, 0x00, 0x62, 0x64, 0x60, 0x00, 0x21, 0xe1, 0x00, 0x00,
    0x20, 0x01, 0x00, 0x00, 0x10, 0x02, 0x00, 0x00, 0x0f, 0xfc, 0x00, 0x00,
    0x10, 0x02, 0x1e, 0x00, 0x20, 0x01, 0x21, 0x00, 0x20, 0x01, 0x45, 0x00,
    0x1f, 0xff, 0x86, 0x00,
};

static const uint8_t cat_blink_map[] = {
    0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0c, 0x03, 0x00, 0x00,
    0x0e, 0x07, 0x00, 0x00, 0x0b, 0x0d, 0x00, 0x00, 0x09, 0xf9, 0x00, 0x00,
    0x10, 0x00, 0x80, 0x00, 0x10, 0x00, 0x80, 0x00, 0x20, 0x00, 0x40, 0x00,
    0x20, 0x00, 0x40, 0x00, 0x26, 0x06, 0x40, 0x00, 0x20, 0x00, 0x40, 0x00,
    0xa0, 0x60, 0x50, 0x00, 0x62, 0x64, 0x60, 0x00, 0x21, 0xe1, 0x00, 0x00,
    0x20, 0x01, 0x00, 0x00, 0x10, 0x02, 0x00, 0x00, 0x0f, 0xfc, 0x00, 0x00,
    0x10, 0x02, 0x1e, 0x00, 0x20, 0x01, 0x21, 0x00, 0x20, 0x01, 0x45, 0x00,
    0x1f, 0xff, 0x86, 0x00,
};

static const lv_img_dsc_t cat_open = {
    .header.cf = LV_COLOR_FORMAT_I1,
    .header.w = CAT_W,
    .header.h = CAT_H,
    .data_size = sizeof(cat_open_map),
    .data = cat_open_map,
};

static const lv_img_dsc_t cat_blink = {
    .header.cf = LV_COLOR_FORMAT_I1,
    .header.w = CAT_W,
    .header.h = CAT_H,
    .data_size = sizeof(cat_blink_map),
    .data = cat_blink_map,
};

/* ── Periodic update: blink the cat, refresh the layer label ── */

#define TICK_MS 150
#define BLINK_PERIOD_TICKS 24 /* blink for one tick every ~3.6 s */

static lv_obj_t *cat_img;
static lv_obj_t *layer_label;

static void tick_cb(lv_timer_t *timer) {
    static uint32_t tick;
    static uint8_t shown_layer = 0xFF;

    tick++;
    lv_image_set_src(cat_img, (tick % BLINK_PERIOD_TICKS) == 0 ? &cat_blink : &cat_open);

    uint8_t layer = corne_synced_layer();
    if (layer != shown_layer) {
        shown_layer = layer;
        if (layer < ARRAY_SIZE(layer_names)) {
            lv_label_set_text(layer_label, layer_names[layer]);
        } else {
            lv_label_set_text_fmt(layer_label, "L%u", layer);
        }
    }
}

/* ── Screen layout (128x32) ──
 *
 * +-----------+----------------+---------+
 * | BT status |                | Battery |
 * +-----------+----------------+---------+
 * |  (cat)    |     LAYER      |         |
 * +-----------+----------------+---------+
 */

static struct zmk_widget_battery_status battery_widget;
static struct zmk_widget_peripheral_status peripheral_widget;

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);

    zmk_widget_peripheral_status_init(&peripheral_widget, screen);
    lv_obj_align(zmk_widget_peripheral_status_obj(&peripheral_widget), LV_ALIGN_TOP_LEFT, 0, 0);

    zmk_widget_battery_status_init(&battery_widget, screen);
    lv_obj_align(zmk_widget_battery_status_obj(&battery_widget), LV_ALIGN_TOP_RIGHT, 0, 0);

    cat_img = lv_img_create(screen);
    lv_image_set_src(cat_img, &cat_open);
    lv_obj_align(cat_img, LV_ALIGN_BOTTOM_LEFT, 2, 0);

    layer_label = lv_label_create(screen);
    lv_label_set_text(layer_label, layer_names[0]);
    lv_obj_align(layer_label, LV_ALIGN_BOTTOM_MID, 16, -2);

    lv_timer_create(tick_cb, TICK_MS, NULL);

    return screen;
}
