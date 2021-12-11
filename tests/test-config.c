#if !defined(_DEBUG)
 #define _DEBUG
#endif
#undef NDEBUG

#include "../log.h"

#include "../config.c"

#define ALEN(v) (sizeof(v) / sizeof((v)[0]))

/*
 * Stubs
 */

void
user_notification_add_fmt(user_notifications_t *notifications,
                          enum user_notification_kind kind,
                          const char *fmt, ...)
{
}

static void
test_invalid_key(struct context *ctx, bool (*parse_fun)(struct context *ctx),
                 const char *key)
{
    ctx->key = key;
    ctx->value = "value for invalid key";

    if (parse_fun(ctx)) {
        BUG("[%s].%s: did not fail to parse as expected"
            "(key should be invalid)", ctx->section, ctx->key);
    }
}

static void
test_string(struct context *ctx, bool (*parse_fun)(struct context *ctx),
             const char *key, char *const *conf_ptr)
{
    ctx->key = key;

    static const struct {
        const char *option_string;
        const char *value;
        bool invalid;
    } input[] = {
        {"a string", "a string"},
    };

    for (size_t i = 0; i < ALEN(input); i++) {
        ctx->value = input[i].option_string;

        if (input[i].invalid) {
            if (parse_fun(ctx)) {
                BUG("[%s].%s=%s: did not fail to parse as expected",
                    ctx->section, ctx->key, ctx->value);
            }
        } else {
            if (!parse_fun(ctx)) {
                BUG("[%s].%s=%s: failed to parse",
                    ctx->section, ctx->key, ctx->value);
            }
            if (strcmp(*conf_ptr, input[i].value) != 0) {
                BUG("[%s].%s=%s: set value (%s) not the expected one (%s)",
                    ctx->section, ctx->key, ctx->value,
                    *conf_ptr, input[i].value);
            }
        }
    }
}

static void
test_wstring(struct context *ctx, bool (*parse_fun)(struct context *ctx),
             const char *key, wchar_t *const *conf_ptr)
{
    ctx->key = key;

    static const struct {
        const char *option_string;
        const wchar_t *value;
        bool invalid;
    } input[] = {
        {"a string", L"a string"},
    };

    for (size_t i = 0; i < ALEN(input); i++) {
        ctx->value = input[i].option_string;

        if (input[i].invalid) {
            if (parse_fun(ctx)) {
                BUG("[%s].%s=%s: did not fail to parse as expected",
                    ctx->section, ctx->key, ctx->value);
            }
        } else {
            if (!parse_fun(ctx)) {
                BUG("[%s].%s=%s: failed to parse",
                    ctx->section, ctx->key, ctx->value);
            }
            if (wcscmp(*conf_ptr, input[i].value) != 0) {
                BUG("[%s].%s=%s: set value (%ls) not the expected one (%ls)",
                    ctx->section, ctx->key, ctx->value,
                    *conf_ptr, input[i].value);
            }
        }
    }
}

static void
test_boolean(struct context *ctx, bool (*parse_fun)(struct context *ctx),
             const char *key, const bool *conf_ptr)
{
    ctx->key = key;

    static const struct {
        const char *option_string;
        bool value;
        bool invalid;
    } input[] = {
        {"1", true}, {"0", false},
        {"on", true}, {"off", false},
        {"true", true}, {"false", false},
        {"unittest-invalid-boolean-value", false, true},
    };

    for (size_t i = 0; i < ALEN(input); i++) {
        ctx->value = input[i].option_string;

        if (input[i].invalid) {
            if (parse_fun(ctx)) {
                BUG("[%s].%s=%s: did not fail to parse as expected",
                    ctx->section, ctx->key, ctx->value);
            }
        } else {
            if (!parse_fun(ctx)) {
                BUG("[%s].%s=%s: failed to parse",
                    ctx->section, ctx->key, ctx->value);
            }
            if (*conf_ptr != input[i].value) {
                BUG("[%s].%s=%s: set value (%s) not the expected one (%s)",
                    ctx->section, ctx->key, ctx->value,
                    *conf_ptr ? "true" : "false",
                    input[i].value ? "true" : "false");
            }
        }
    }
}

static void
test_uint16(struct context *ctx, bool (*parse_fun)(struct context *ctx),
            const char *key, const uint16_t *conf_ptr)
{
    ctx->key = key;

    static const struct {
        const char *option_string;
        uint16_t value;
        bool invalid;
    } input[] = {
        {"0", 0}, {"65535", 65535}, {"65536", 0, true},
        {"abc", 0, true}, {"true", 0, true},
    };

    for (size_t i = 0; i < ALEN(input); i++) {
        ctx->value = input[i].option_string;

        if (input[i].invalid) {
            if (parse_fun(ctx)) {
                BUG("[%s].%s=%s: did not fail to parse as expected",
                    ctx->section, ctx->key, ctx->value);
            }
        } else {
            if (!parse_fun(ctx)) {
                BUG("[%s].%s=%s: failed to parse",
                    ctx->section, ctx->key, ctx->value);
            }
            if (*conf_ptr != input[i].value) {
                BUG("[%s].%s=%s: set value (%hu) not the expected one (%hu)",
                    ctx->section, ctx->key, ctx->value,
                    *conf_ptr, input[i].value);
            }
        }
    }
}

static void
test_pt_or_px(struct context *ctx, bool (*parse_fun)(struct context *ctx),
              const char *key, const struct pt_or_px *conf_ptr)
{
    ctx->key = key;

    static const struct {
        const char *option_string;
        struct pt_or_px value;
        bool invalid;
    } input[] = {
        {"12", {.pt = 12}}, {"12px", {.px = 12}},
        {"unittest-invalid-pt-or-px-value", {0}, true},
    };

    for (size_t i = 0; i < ALEN(input); i++) {
        ctx->value = input[i].option_string;

        if (input[i].invalid) {
            if (parse_fun(ctx)) {
                BUG("[%s].%s=%s: did not fail to parse as expected",
                    ctx->section, ctx->key, ctx->value);
            }
        } else {
            if (!parse_fun(ctx)) {
                BUG("[%s].%s=%s: failed to parse",
                    ctx->section, ctx->key, ctx->value);
            }
            if (memcmp(conf_ptr, &input[i].value, sizeof(*conf_ptr)) != 0) {
                BUG("[%s].%s=%s: "
                    "set value (pt=%f, px=%d) not the expected one (pt=%f, px=%d)",
                    ctx->section, ctx->key, ctx->value,
                    conf_ptr->pt, conf_ptr->px,
                    input[i].value.pt, input[i].value.px);
            }
        }
    }
}

static void
test_section_main(void)
{
    struct config conf = {0};
    struct context ctx = {.conf = &conf, .section = "main", .path = "unittest"};

    test_invalid_key(&ctx, &parse_section_main, "invalid-key");

    test_string(&ctx, &parse_section_main, "shell", &conf.shell);
    test_string(&ctx, &parse_section_main, "term", &conf.term);
    test_string(&ctx, &parse_section_main, "app-id", &conf.app_id);

    test_wstring(&ctx, &parse_section_main, "word-delimiters", &conf.word_delimiters);

    test_boolean(&ctx, &parse_section_main, "login-shell", &conf.login_shell);
    test_boolean(&ctx, &parse_section_main, "box-drawings-uses-font-glyphs", &conf.box_drawings_uses_font_glyphs);
    test_boolean(&ctx, &parse_section_main, "locked-title", &conf.locked_title);
    test_boolean(&ctx, &parse_section_main, "notify-focus-inhibit", &conf.notify_focus_inhibit);

    test_pt_or_px(&ctx, &parse_section_main, "line-height", &conf.line_height);
    test_pt_or_px(&ctx, &parse_section_main, "letter-spacing", &conf.letter_spacing);
    test_pt_or_px(&ctx, &parse_section_main, "horizontal-letter-offset", &conf.horizontal_letter_offset);
    test_pt_or_px(&ctx, &parse_section_main, "vertical-letter-offset", &conf.vertical_letter_offset);

    test_uint16(&ctx, &parse_section_main, "resize-delay-ms", &conf.resize_delay_ms);
    test_uint16(&ctx, &parse_section_main, "workers", &conf.render_worker_count);

    /* TODO: font (custom) */
    /* TODO: include (custom) */
    /* TODO: dpi-aware (enum/boolean) */
    /* TODO: bold-text-in-bright (enum/boolean) */
    /* TODO: pad (geometry + optional string)*/
    /* TODO: initial-window-size-pixels (geometry) */
    /* TODO: initial-window-size-chars (geometry) */
    /* TODO: notify (spawn template)*/
    /* TODO: selection-target (enum) */
    /* TODO: initial-window-mode (enum) */

    config_free(conf);
}

static void
test_key_binding(struct context *ctx, bool (*parse_fun)(struct context *ctx),
                 int action, int max_action, const char *const *map,
                 struct config_key_binding_list *bindings)
{
    xassert(map[action] != NULL);
    xassert(bindings->count == 0);

    const char *key = map[action];

    /* “Randomize” which modifiers to enable */
    const bool ctrl = action % 2;
    const bool alt = action % 3;
    const bool shift = action % 4;
    const bool super = action % 5;

    /* Generate the modifier part of the ‘value’ */
    char modifier_string[32];
    int chars = sprintf(modifier_string, "%s%s%s%s",
                        ctrl ? XKB_MOD_NAME_CTRL "+" : "",
                        alt ? XKB_MOD_NAME_ALT "+" : "",
                        shift ? XKB_MOD_NAME_SHIFT "+" : "",
                        super ? XKB_MOD_NAME_LOGO "+" : "");

    /* Use a unique symbol for this action */
    xkb_keysym_t sym = XKB_KEY_a + action;
    char sym_name[8];
    xkb_keysym_get_name(sym, sym_name, sizeof(sym_name));

    /* Finally, generate the ‘value’ (e.g. “Control+shift+x”) */
    char value[chars + strlen(sym_name) + 1];
    sprintf(value, "%s%s", modifier_string, sym_name);

    ctx->key = key;
    ctx->value = value;

    if (!parse_fun(ctx)) {
        BUG("[%s].%s=%s failed to parse",
            ctx->section, ctx->key, ctx->value);
    }

    const struct config_key_binding *binding =
        &bindings->arr[bindings->count - 1];

    xassert(binding->pipe.argv.args == NULL);

    if (binding->action != action) {
        BUG("[%s].%s=%s: action mismatch: %d != %d",
            ctx->section, ctx->key, ctx->value, binding->action, action);
    }

    if (binding->sym != sym) {
        BUG("[%s].%s=%s: key symbol mismatch: %d != %d",
            ctx->section, ctx->key, ctx->value, binding->sym, sym);
    }

    if (binding->modifiers.ctrl != ctrl ||
        binding->modifiers.alt != alt ||
        binding->modifiers.shift != shift ||
        binding->modifiers.meta != super)
    {
        BUG("[%s].%s=%s: modifier mismatch:\n"
            "  have:     ctrl=%d, alt=%d, shift=%d, super=%d\n"
            "  expected: ctrl=%d, alt=%d, shift=%d, super=%d",
            ctx->section, ctx->key, ctx->value,
            binding->modifiers.ctrl, binding->modifiers.alt,
            binding->modifiers.shift, binding->modifiers.meta,
            ctrl, alt, shift, super);
    }

    key_binding_list_free(bindings);
    bindings->arr = NULL;
    bindings->count = 0;

    if (action >= max_action)
        return;

    /*
     * Test collisions
     */

    /* First, verify we get a collision when trying to assign the same
     * key combo to multiple actions */
    bindings->count = 1;
    bindings->arr = xmalloc(sizeof(bindings->arr[0]));
    bindings->arr[0] = (struct config_key_binding){
        .action = action + 1,
        .sym = XKB_KEY_a,
        .modifiers = {
            .ctrl = true,
        },
    };

    xkb_keysym_get_name(XKB_KEY_a, sym_name, sizeof(sym_name));

    char collision[128];
    snprintf(collision, sizeof(collision), "%s+%s", XKB_MOD_NAME_CTRL, sym_name);

    ctx->value = collision;
    if (parse_fun(ctx)) {
        BUG("[%s].%s=%s: key combo collision not detected",
            ctx->section, ctx->key, ctx->value);
    }

    /* Next, verify we get a collision when trying to assign the same
     * key combo to the same action, but with different pipe argvs */
    bindings->arr[0].action = action;
    bindings->arr[0].pipe.master_copy = true;
    bindings->arr[0].pipe.argv.args = xmalloc(4 * sizeof(bindings->arr[0].pipe.argv.args[0]));
    bindings->arr[0].pipe.argv.args[0] = xstrdup("/usr/bin/foobar");
    bindings->arr[0].pipe.argv.args[1] = xstrdup("hello");
    bindings->arr[0].pipe.argv.args[2] = xstrdup("world");
    bindings->arr[0].pipe.argv.args[3] = NULL;

    snprintf(collision, sizeof(collision),
             "[/usr/bin/foobar hello] %s+%s",
             XKB_MOD_NAME_CTRL, sym_name);

    ctx->value = collision;
    if (parse_fun(ctx)) {
        BUG("[%s].%s=%s: key combo collision not detected",
            ctx->section, ctx->key, ctx->value);
    }

    /* Finally, verify we do *not* get a collision when assigning the
     * same key combo to the same action, with matching argvs */
    snprintf(collision, sizeof(collision),
             "[/usr/bin/foobar hello world] %s+%s",
             XKB_MOD_NAME_CTRL, sym_name);

    ctx->value = collision;
    if (!parse_fun(ctx)) {
        BUG("[%s].%s=%s: invalid key combo collision",
            ctx->section, ctx->key, ctx->value);
    }

    key_binding_list_free(bindings);
    bindings->arr = NULL;
    bindings->count = 0;
}

static void
test_mouse_binding(struct context *ctx, bool (*parse_fun)(struct context *ctx),
                   int action, int max_action, const char *const *map,
                   struct config_mouse_binding_list *bindings)
{
    xassert(map[action] != NULL);
    xassert(bindings->count == 0);

    const char *key = map[action];

    /* “Randomize” which modifiers to enable */
    const bool ctrl = action % 2;
    const bool alt = action % 3;
    const bool shift = action % 4;
    const bool super = action % 5;

    /* Generate the modifier part of the ‘value’ */
    char modifier_string[32];
    int chars = sprintf(modifier_string, "%s%s%s%s",
                        ctrl ? XKB_MOD_NAME_CTRL "+" : "",
                        alt ? XKB_MOD_NAME_ALT "+" : "",
                        shift ? XKB_MOD_NAME_SHIFT "+" : "",
                        super ? XKB_MOD_NAME_LOGO "+" : "");

    const int button_idx = action % ALEN(button_map);
    const int button = button_map[button_idx].code;
    const char *const button_name = button_map[button_idx].name;
    const int click_count = action % 3 + 1;

    xassert(click_count > 0);

    /* Finally, generate the ‘value’ (e.g. “Control+shift+x”) */
    char value[chars + strlen(button_name) + 2 + 1];
    chars = sprintf(value, "%s%s", modifier_string, button_name);
    if (click_count > 1)
        sprintf(&value[chars], "-%d", click_count);

    ctx->key = key;
    ctx->value = value;

    if (!parse_fun(ctx)) {
        BUG("[%s].%s=%s failed to parse",
            ctx->section, ctx->key, ctx->value);
    }

    const struct config_mouse_binding *binding =
        &bindings->arr[bindings->count - 1];

    xassert(binding->pipe.argv.args == NULL);

    if (binding->action != action) {
        BUG("[%s].%s=%s: action mismatch: %d != %d",
            ctx->section, ctx->key, ctx->value, binding->action, action);
    }

    if (binding->button != button) {
        BUG("[%s].%s=%s: button mismatch: %d != %d",
            ctx->section, ctx->key, ctx->value, binding->button, button);
    }

    if (binding->count != click_count) {
        BUG("[%s].%s=%s: button click count mismatch: %d != %d",
            ctx->section, ctx->key, ctx->value, binding->count, click_count);
    }

    if (binding->modifiers.ctrl != ctrl ||
        binding->modifiers.alt != alt ||
        binding->modifiers.shift != shift ||
        binding->modifiers.meta != super)
    {
        BUG("[%s].%s=%s: modifier mismatch:\n"
            "  have:     ctrl=%d, alt=%d, shift=%d, super=%d\n"
            "  expected: ctrl=%d, alt=%d, shift=%d, super=%d",
            ctx->section, ctx->key, ctx->value,
            binding->modifiers.ctrl, binding->modifiers.alt,
            binding->modifiers.shift, binding->modifiers.meta,
            ctrl, alt, shift, super);
    }

    mouse_binding_list_free(bindings);
    bindings->arr = NULL;
    bindings->count = 0;

    if (action >= max_action)
        return;

    /*
     * Test collisions
     */

    /* First, verify we get a collision when trying to assign the same
     * key combo to multiple actions */
    bindings->count = 1;
    bindings->arr = xmalloc(sizeof(bindings->arr[0]));
    bindings->arr[0] = (struct config_mouse_binding){
        .action = action + 1,
        .button = BTN_LEFT,
        .count = 1,
        .modifiers = {
            .ctrl = true,
        },
    };

    char collision[128];
    snprintf(collision, sizeof(collision), "%s+BTN_LEFT", XKB_MOD_NAME_CTRL);

    ctx->value = collision;
    if (parse_fun(ctx)) {
        BUG("[%s].%s=%s: mouse combo collision not detected",
            ctx->section, ctx->key, ctx->value);
    }

    /* Next, verify we get a collision when trying to assign the same
     * key combo to the same action, but with different pipe argvs */
    bindings->arr[0].action = action;
    bindings->arr[0].pipe.master_copy = true;
    bindings->arr[0].pipe.argv.args = xmalloc(4 * sizeof(bindings->arr[0].pipe.argv.args[0]));
    bindings->arr[0].pipe.argv.args[0] = xstrdup("/usr/bin/foobar");
    bindings->arr[0].pipe.argv.args[1] = xstrdup("hello");
    bindings->arr[0].pipe.argv.args[2] = xstrdup("world");
    bindings->arr[0].pipe.argv.args[3] = NULL;

    snprintf(collision, sizeof(collision),
             "[/usr/bin/foobar hello] %s+BTN_LEFT", XKB_MOD_NAME_CTRL);

    ctx->value = collision;
    if (parse_fun(ctx)) {
        BUG("[%s].%s=%s: key combo collision not detected",
            ctx->section, ctx->key, ctx->value);
    }

#if 0 /* BUG! (should be fixed by https://codeberg.org/dnkl/foot/pulls/832) */
    /* Finally, verify we do *not* get a collision when assigning the
     * same key combo to the same action, with matching argvs */
    snprintf(collision, sizeof(collision),
             "[/usr/bin/foobar hello world] %s+BTN_LEFT", XKB_MOD_NAME_CTRL);

    ctx->value = collision;
    if (!parse_fun(ctx)) {
        BUG("[%s].%s=%s: invalid mouse combo collision",
            ctx->section, ctx->key, ctx->value);
    }
#endif
    mouse_binding_list_free(bindings);
    bindings->arr = NULL;
    bindings->count = 0;
}

static void
test_section_key_bindings(void)
{
    struct config conf = {0};
    struct context ctx = {
        .conf = &conf, .section = "key-bindings", .path = "unittest"};

    test_invalid_key(&ctx, &parse_section_key_bindings, "invalid-key");

    for (int action = 0; action < BIND_ACTION_KEY_COUNT; action++) {
        if (binding_action_map[action] == NULL)
            continue;

        test_key_binding(
            &ctx, &parse_section_key_bindings,
            action, BIND_ACTION_KEY_COUNT - 1,
            binding_action_map, &conf.bindings.key);
    }

    config_free(conf);
}

#if 0
static void
test_section_search_bindings(void)
{
    struct config conf = {0};
    struct context ctx = {
        .conf = &conf, .section = "search-bindings", .path = "unittest"};

    test_invalid_key(&ctx, &parse_section_search_bindings, "invalid-key");

    for (int action = 0; action < BIND_ACTION_SEARCH_COUNT; action++) {
        if (search_binding_action_map[action] == NULL)
            continue;

        test_key_binding(
            &ctx, &parse_section_search_bindings,
            action, BIND_ACTION_SEARCH_COUNT - 1,
            search_binding_action_map, &conf.bindings.search);
    }

    config_free(conf);
}

static void
test_section_url_bindings(void)
{
    struct config conf = {0};
    struct context ctx = {
        .conf = &conf, .section = "rul-bindings", .path = "unittest"};

    test_invalid_key(&ctx, &parse_section_url_bindings, "invalid-key");

    for (int action = 0; action < BIND_ACTION_URL_COUNT; action++) {
        if (url_binding_action_map[action] == NULL)
            continue;

        test_key_binding(
            &ctx, &parse_section_url_bindings,
            action, BIND_ACTION_URL_COUNT - 1,
            url_binding_action_map, &conf.bindings.url);
    }

    config_free(conf);
}
#endif

static void
test_section_mouse_bindings(void)
{
    struct config conf = {0};
    struct context ctx = {
        .conf = &conf, .section = "mouse-bindings", .path = "unittest"};

    test_invalid_key(&ctx, &parse_section_mouse_bindings, "invalid-key");

    for (int action = 0; action < BIND_ACTION_COUNT; action++) {
        if (binding_action_map[action] == NULL)
            continue;

        test_mouse_binding(
            &ctx, &parse_section_mouse_bindings,
            action, BIND_ACTION_COUNT - 1,
            binding_action_map, &conf.bindings.mouse);
    }

    config_free(conf);
}

int
main(int argc, const char *const *argv)
{
    log_init(LOG_COLORIZE_AUTO, false, 0, LOG_CLASS_ERROR);
    test_section_main();
    test_section_key_bindings();
#if 0
    test_section_search_bindings();
    test_section_url_bindings();
#endif
    test_section_mouse_bindings();
    log_deinit();
    return 0;
}
