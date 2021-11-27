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

int
main(int argc, const char *const *argv)
{
    log_init(LOG_COLORIZE_AUTO, false, 0, LOG_CLASS_ERROR);
    test_section_main();
    log_deinit();
    return 0;
}
