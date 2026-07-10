/*
 * Layer-forward behavior.
 *
 * ZMK does not sync layer state to split peripherals, so the right-half
 * display cannot know the active layer on its own. This behavior has
 * BEHAVIOR_LOCALITY_GLOBAL, which makes the central forward every
 * invocation to all peripherals over the split transport. A listener on
 * the central invokes it with the highest active layer as param1 whenever
 * layer state changes; the peripheral's handler stores the value for the
 * status screen to read.
 *
 * The devicetree node name must stay within the split transport's
 * behavior-name limit (9 bytes including the terminator).
 */

#define DT_DRV_COMPAT corne_behavior_layer_fwd

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <zmk/behavior.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static uint8_t synced_layer;

uint8_t corne_synced_layer(void) { return synced_layer; }

static int on_layer_fwd_binding_pressed(struct zmk_behavior_binding *binding,
                                        struct zmk_behavior_binding_event event) {
    synced_layer = (uint8_t)binding->param1;
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_layer_fwd_binding_released(struct zmk_behavior_binding *binding,
                                         struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api layer_fwd_driver_api = {
    .binding_pressed = on_layer_fwd_binding_pressed,
    .binding_released = on_layer_fwd_binding_released,
    .locality = BEHAVIOR_LOCALITY_GLOBAL,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
#endif
};

#define LAYER_FWD_INST(n)                                                                          \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &layer_fwd_driver_api);

DT_INST_FOREACH_STATUS_OKAY(LAYER_FWD_INST)

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>

static int layer_fwd_listener(const zmk_event_t *eh) {
    static uint8_t last_sent = 0xFF;

    uint8_t highest = zmk_keymap_highest_layer_active();
    if (highest == last_sent) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    last_sent = highest;

    struct zmk_behavior_binding binding = {
        .behavior_dev = DEVICE_DT_NAME(DT_DRV_INST(0)),
        .param1 = highest,
    };
    struct zmk_behavior_binding_event event = {
        .layer = highest,
        .position = 0,
        .timestamp = k_uptime_get(),
    };
    zmk_behavior_invoke_binding(&binding, event, true);

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(layer_fwd, layer_fwd_listener);
ZMK_SUBSCRIPTION(layer_fwd, zmk_layer_state_changed);

#endif /* IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) */

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
