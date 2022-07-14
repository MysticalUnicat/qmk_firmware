// per frame, background effect, foreground effect, hue, saturation, value (always scaled by settings value)

#define UNICAT_MODES(MODE) \
    MODE(RAINDROP, READ,              ZERO,          BACK, SAT,           MAX)       \
    MODE(RAINDROP, READ,              REACTIVE_WIDE, BACK, SAT,           FORE_HIGH) \
    MODE(RAINDROP, READ,              REACTIVE_WIDE, BACK, FORE_AVG_SAT,  MAX)       \
    MODE(NONE,     CYCLE_IN_OUT_DUAL, ZERO,          BACK, SAT,           MAX)

#define UNICAT_X(FRAME, BACK, FORE, H, S, V) RGB_MATRIX_EFFECT(UNICAT_ ## FRAME ## _ ## BACK ## _ ## FORE ## _ ## H ## _ ## S ## _ ## _ ## V)
UNICAT_MODES(UNICAT_X)
#undef UNICAT_X

#ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS

static bool UNICAT_runner(effect_params_t * params, HSV (*gen_fn)(uint8_t index)) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();
        HSV hsv = gen_fn(i);
        hsv.v = scale8(hsv.v, rgb_matrix_config.hsv.v);
        RGB rgb = rgb_matrix_hsv_to_rgb(hsv);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

// once per frame
extern uint8_t g_unicat_rgb[RGBLED_NUM];

#define UNICAT_FRAME_NONE_init() do { } while(0)
#define UNICAT_FRAME_NONE_frame() do { } while(0)

static void UNICAT_FRAME_RAINDROP_set(int i) {
    // Take the shortest path between hues
    int16_t deltaH = ((rgb_matrix_config.hsv.h + 180) % 360 - rgb_matrix_config.hsv.h) / 4;
    if (deltaH > 127) {
        deltaH -= 256;
    } else if (deltaH < -127) {
        deltaH += 256;
    }
    g_unicat_rgb[i] = rgb_matrix_config.hsv.h + (deltaH * (rand() & 0x03));
}
static void UNICAT_FRAME_RAINDROP_init(void) {
    for(int i = 0; i < RGBLED_NUM; i++) {
        UNICAT_FRAME_RAINDROP_set(i);
    }
}
static void UNICAT_FRAME_RAINDROP_frame(void) {
    if (scale16by8(g_rgb_timer, qadd8(rgb_matrix_config.speed, 16)) % 10 == 0) {
        UNICAT_FRAME_RAINDROP_set(rand() % RGBLED_NUM);
    }
}

// from rgb matrix animation runners
#define UNICAT_DX_DY_TIME(index) \
    int16_t dx = g_led_config.point[index].x - k_rgb_matrix_center.x; \
    int16_t dy = g_led_config.point[index].y - k_rgb_matrix_center.y; \
    uint8_t time = scale16by8(g_rgb_timer, rgb_matrix_config.speed / 2);

typedef uint8_t (*unicat_reactive_splash_f)(uint8_t v, int16_t dx, int16_t dy, uint8_t dist, uint16_t tick);
uint8_t unicat_reactive_splash(uint8_t index, uint8_t start, unicat_reactive_splash_f effect_func) {
    uint8_t count = g_last_hit_tracker.count;
    uint8_t v = 31;
    for (uint8_t j = start; j < count; j++) {
        int16_t  dx   = g_led_config.point[index].x - g_last_hit_tracker.x[j];
        int16_t  dy   = g_led_config.point[index].y - g_last_hit_tracker.y[j];
        uint8_t  dist = sqrt16(dx * dx + dy * dy);
        uint16_t tick = scale16by8(g_last_hit_tracker.tick[j], qadd8(rgb_matrix_config.speed, 1));
        v = effect_func(v, dx, dy, dist, tick);
    }
    return v;
}

// background and foreground data generators
#define UNICAT_DATA_CYCLE_IN_OUT_DUAL_init() do { } while(0)
static uint8_t UNICAT_DATA_CYCLE_IN_OUT_DUAL_frame(uint8_t index) {
    UNICAT_DX_DY_TIME(index)
    dx = (k_rgb_matrix_center.x / 2) - abs8(dx);
    return 3 * sqrt16(dx * dx + dy * dy) + time;
}

static uint8_t UNICAT_DATA_REACTIVE_WIDE_math(uint8_t v, int16_t dx, int16_t dy, uint8_t dist, uint16_t tick) {
    uint16_t effect = tick + dist * 5;
    if (effect > 255) effect = 255;
    return qadd8(v, 255 - effect);
}
#define UNICAT_DATA_REACTIVE_WIDE_init() do { } while(0)
#define UNICAT_DATA_REACTIVE_WIDE_frame(index) unicat_reactive_splash(index, 0, UNICAT_DATA_REACTIVE_WIDE_math)

#define UNICAT_DATA_READ_init() do { } while(0)
#define UNICAT_DATA_READ_frame(index) g_unicat_rgb[index]

#define UNICAT_DATA_ZERO_init() do { } while(0)
#define UNICAT_DATA_ZERO_frame(index) 0

// HSV construction
#define UNICAT_MIX_BACK         back
#define UNICAT_MIX_FORE         fore
#define UNICAT_MIX_FORE_HIGH    avg8(fore, 255)
#define UNICAT_MIX_FORE_AVG_SAT avg8(fore, rgb_matrix_config.hsv.s)
#define UNICAT_MIX_SAT          rgb_matrix_config.hsv.s
#define UNICAT_MIX_MAX          255

#define UNICAT_X(FRAME, BACK, FORE, H, S, V) \
    static HSV UNICAT_ ## FRAME ## _ ## BACK ## _ ## FORE ## _ ## H ## _ ## S ## _ ## _ ## V ## _gen(uint8_t index) { \
        uint8_t back = UNICAT_DATA_ ## BACK ## _frame(index); \
        uint8_t fore = UNICAT_DATA_ ## FORE ## _frame(index); \
        (void)back; \
        (void)fore; \
        return (HSV) { UNICAT_MIX_ ## H, UNICAT_MIX_ ## S, UNICAT_MIX_ ## V }; \
    } \
    bool UNICAT_ ## FRAME ## _ ## BACK ## _ ## FORE ## _ ## H ## _ ## S ## _ ## _ ## V (effect_params_t* params) { \
        if(params->init) { \
            UNICAT_FRAME_ ## FRAME ## _init(); \
            UNICAT_DATA_ ## BACK ## _init(); \
            UNICAT_DATA_ ## FORE ## _init(); \
        } \
        UNICAT_FRAME_ ## FRAME ## _frame(); \
        return UNICAT_runner(params, UNICAT_ ## FRAME ## _ ## BACK ## _ ## FORE ## _ ## H ## _ ## S ## _ ## _ ## V ## _gen); \
    }
UNICAT_MODES(UNICAT_X)
#undef UNICAT_X

#endif
