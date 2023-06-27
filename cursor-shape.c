#include <stdlib.h>

#include "cursor-shape.h"
#include "debug.h"
#include "util.h"

const char *
cursor_shape_to_string(enum cursor_shape shape)
{
    static const char *const table[] = {
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
    return table[shape];
}
