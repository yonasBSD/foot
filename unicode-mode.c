#include "unicode-mode.h"

#define LOG_MODULE "unicode-input"
#define LOG_ENABLE_DBG 0
#include "log.h"
#include "render.h"
#include "search.h"

void
unicode_mode_activate(struct terminal *term)
{
    if (term->unicode_mode.active)
        return;

    term->unicode_mode.active = true;
    term->unicode_mode.character = u'\0';
    term->unicode_mode.count = 0;
    unicode_mode_updated(term);
}

void
unicode_mode_deactivate(struct terminal *term)
{
    if (!term->unicode_mode.active)
        return;

    term->unicode_mode.active = false;
    unicode_mode_updated(term);
}

void
unicode_mode_updated(struct terminal *term)
{
    if (term == NULL)
        return;

    if (term->is_searching)
        render_refresh_search(term);
    else
        render_refresh(term);
}

void
unicode_mode_input(struct seat *seat, struct terminal *term,
                   xkb_keysym_t sym)
{
    if (sym == XKB_KEY_Return ||
        sym == XKB_KEY_space ||
        sym == XKB_KEY_KP_Enter ||
        sym == XKB_KEY_KP_Space)
    {
        char utf8[MB_CUR_MAX];
        size_t chars = c32rtomb(
            utf8, term->unicode_mode.character, &(mbstate_t){0});

        LOG_DBG("Unicode input: 0x%06x -> %.*s",
                term->unicode_mode.character, (int)chars, utf8);

        if (chars != (size_t)-1) {
            if (term->is_searching)
                search_add_chars(term, utf8, chars);
            else
                term_to_slave(term, utf8, chars);
        }

        unicode_mode_deactivate(term);
    }

    else if (sym == XKB_KEY_Escape ||
             sym == XKB_KEY_q ||
             (seat->kbd.ctrl && (sym == XKB_KEY_c ||
                                 sym == XKB_KEY_d ||
                                 sym == XKB_KEY_g)))
    {
        unicode_mode_deactivate(term);
    }

    else if (sym == XKB_KEY_BackSpace) {
        if (term->unicode_mode.count > 0) {
            term->unicode_mode.character >>= 4;
            term->unicode_mode.count--;
            unicode_mode_updated(term);
        }
    }

    else if (term->unicode_mode.count < 6) {
        int digit = -1;

        /* 0-9, a-f, A-F */
        if (sym >= XKB_KEY_0 && sym <= XKB_KEY_9)
            digit = sym - XKB_KEY_0;
        else if (sym >= XKB_KEY_KP_0 && sym <= XKB_KEY_KP_9)
            digit = sym - XKB_KEY_KP_0;
        else if (sym >= XKB_KEY_a && sym <= XKB_KEY_f)
            digit = 0xa + (sym - XKB_KEY_a);
        else if (sym >= XKB_KEY_A && sym <= XKB_KEY_F)
            digit = 0xa + (sym - XKB_KEY_A);

        if (digit >= 0) {
            xassert(digit >= 0 && digit <= 0xf);
            term->unicode_mode.character <<= 4;
            term->unicode_mode.character |= digit;
            term->unicode_mode.count++;
            unicode_mode_updated(term);
        }
    }
}
