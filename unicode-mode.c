#include "unicode-mode.h"

#include "render.h"

void
unicode_mode_activate(struct seat *seat)
{
    if (seat->unicode_mode.active)
        return;

    seat->unicode_mode.active = true;
    seat->unicode_mode.character = u'\0';
    seat->unicode_mode.count = 0;
    unicode_mode_updated(seat);
}

void
unicode_mode_deactivate(struct seat *seat)
{
    if (!seat->unicode_mode.active)
        return;

    seat->unicode_mode.active = false;
    unicode_mode_updated(seat);
}

void
unicode_mode_updated(struct seat *seat)
{
    struct terminal *term = seat->kbd_focus;
    if (term == NULL)
        return;

    if (term->is_searching)
        render_refresh_search(term);
    else
        render_refresh(term);
}
