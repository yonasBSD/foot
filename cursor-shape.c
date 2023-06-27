#include <stdlib.h>

#include "cursor-shape.h"
#include "debug.h"
#include "util.h"

const char *
cursor_shape_to_string(enum cursor_shape shape)
{
    static const char *const table[CURSOR_SHAPE_COUNT] = {
        [CURSOR_SHAPE_NONE] = NULL,
        [CURSOR_SHAPE_HIDDEN] = "hidden",
        [CURSOR_SHAPE_LEFT_PTR] = "left_ptr",
        [CURSOR_SHAPE_TEXT] = "text",
        [CURSOR_SHAPE_TEXT_FALLBACK] = "xterm",
        [CURSOR_SHAPE_TOP_LEFT_CORNER] = "top_left_corner",
        [CURSOR_SHAPE_TOP_RIGHT_CORNER] = "top_right_corner",
        [CURSOR_SHAPE_BOTTOM_LEFT_CORNER] = "bottom_left_corner",
        [CURSOR_SHAPE_BOTTOM_RIGHT_CORNER] = "bottom_right_corner",
        [CURSOR_SHAPE_LEFT_SIDE] = "left_side",
        [CURSOR_SHAPE_RIGHT_SIDE] = "right_side",
        [CURSOR_SHAPE_TOP_SIDE] = "top_side",
        [CURSOR_SHAPE_BOTTOM_SIDE] = "bottom_side",

    };

    xassert(shape <= ALEN(table));
    xassert(table[shape] != NULL);
    return table[shape];
}

#if defined(HAVE_CURSOR_SHAPE)
enum wp_cursor_shape_device_v1_shape
cursor_shape_to_server_shape(enum cursor_shape shape)
{
    static const enum wp_cursor_shape_device_v1_shape table[CURSOR_SHAPE_COUNT] = {
        [CURSOR_SHAPE_LEFT_PTR] = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT,
        [CURSOR_SHAPE_TEXT] = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_TEXT,
        [CURSOR_SHAPE_TEXT_FALLBACK] = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_TEXT,
        [CURSOR_SHAPE_TOP_LEFT_CORNER] = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NW_RESIZE,
        [CURSOR_SHAPE_TOP_RIGHT_CORNER] = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NE_RESIZE,
        [CURSOR_SHAPE_BOTTOM_LEFT_CORNER] = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_SW_RESIZE,
        [CURSOR_SHAPE_BOTTOM_RIGHT_CORNER] = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_SE_RESIZE,
        [CURSOR_SHAPE_LEFT_SIDE] = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_W_RESIZE,
        [CURSOR_SHAPE_RIGHT_SIDE] = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_E_RESIZE,
        [CURSOR_SHAPE_TOP_SIDE] = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_N_RESIZE,
        [CURSOR_SHAPE_BOTTOM_SIDE] = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_S_RESIZE,
    };

    xassert(shape <= ALEN(table));
    xassert(table[shape] != 0);
    return table[shape];
}
#endif
