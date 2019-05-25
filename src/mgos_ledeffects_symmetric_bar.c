#include "mgos.h"
#include "led_master.h"

typedef struct {
    uint8_t rings;
    uint32_t num_pix;
    double mid_pos;
    int mid_pos_cut;
    tools_rgb_data* all_colors;
    tools_rgb_data* shade_colors;
    tools_rgb_data black;
    tools_rgb_data back_ground;
    audio_trigger_data* atd;    
} symmetric_bar_data;

static symmetric_bar_data* sbd;
static bool do_time = false;
static uint32_t max_time = 0;

static tools_rgb_data mgos_intern_symmetric_bar_calc_shade(tools_rgb_data start, tools_rgb_data end, double percent)
{
    tools_rgb_data res;

    double r = (end.r * 1.0) + percent * (start.r - end.r);
    double g = (end.g * 1.0) + percent * (start.g - end.g);
    double b = (end.b * 1.0) + percent * (start.b - end.b);
    double a = (end.a * 1.0) + percent * (start.a - end.a);

    res.r = (int)r > 255.0 ? 255 : (int)r < 0.0 ? 0 : (int)r;
    res.g = (int)g > 255.0 ? 255 : (int)g < 0.0 ? 0 : (int)g;
    res.b = (int)b > 255.0 ? 255 : (int)b < 0.0 ? 0 : (int)b;
    res.a = (int)a > 255.0 ? 255 : (int)a < 0.0 ? 0 : (int)a;
    LOG(LL_VERBOSE_DEBUG, ("Perc:\t%.03f\tStart:\t0x%02X\t0x%02X\t0x%02X\tEnd:\t0x%02X\t0x%02X\t0x%02X\tRes:\t0x%02X\t0x%02X\t0x%02X", percent, start.r, start.g, start.b, end.r, end.g, end.b, res.r, res.g, res.b));

    return res;
}

static void mgos_intern_symmetric_bar_calc_colors(mgos_rgbleds* leds, symmetric_bar_data* sbd)
{
    tools_rgb_data start_color = tools_hexcolor_str_to_rgb((char*)mgos_sys_config_get_ledeffects_symmetric_bar_startcolor());
    tools_rgb_data end_color = tools_hexcolor_str_to_rgb((char*)mgos_sys_config_get_ledeffects_symmetric_bar_endcolor());

    int num_mid_pix = (int)round(sbd->mid_pos);
    for (int i = 0; i < num_mid_pix; i++) {
        double h = 360.0 - ((i * 1.0 / (num_mid_pix - 1)) * 360.0);
        double s = mgos_sys_config_get_ledeffects_symmetric_bar_saturation();
        double v = mgos_sys_config_get_ledeffects_symmetric_bar_value();
        sbd->all_colors[i] = tools_hsv_to_rgb(h, s, v);
        sbd->shade_colors[i] = mgos_intern_symmetric_bar_calc_shade(end_color, start_color, ((i * 1.0 / (num_mid_pix - 1))));
        LOG(LL_VERBOSE_DEBUG, ("ledeffects_symmetric_bar:\tH: %.02f\tS: %.02f\tV: %.02f, #%.02X%.02X%.02X", h, s, v, sbd->all_colors[i].r, sbd->all_colors[i].g, sbd->all_colors[i].b));
    }
    sbd->back_ground = tools_hexcolor_str_to_rgb((char*)mgos_sys_config_get_ledeffects_symmetric_bar_background());
    memset(&sbd->black, 0, sizeof(tools_rgb_data));
}

static void mgos_intern_symmetric_bar_init(mgos_rgbleds* leds)
{
    sbd = calloc(1, sizeof(symmetric_bar_data));

    sbd->atd = (audio_trigger_data*) leds->audio_data;
    sbd->mid_pos = leds->panel_height / 2.0;
    sbd->mid_pos_cut = (leds->panel_height >> 1) - 1;
    sbd->num_pix = leds->panel_width * leds->panel_height;
    sbd->all_colors = calloc(leds->panel_height, sizeof(tools_rgb_data));
    sbd->shade_colors = calloc(leds->panel_height, sizeof(tools_rgb_data));

    mgos_intern_symmetric_bar_calc_colors(leds, sbd);

    leds->timeout = mgos_sys_config_get_ledeffects_symmetric_bar_timeout();
    leds->dim_all = mgos_sys_config_get_ledeffects_symmetric_bar_dim_all();
}

static void mgos_intern_symmetric_bar_exit(mgos_rgbleds* leds)
{
    if (sbd != NULL && sbd->all_colors != NULL) {
        free(sbd->all_colors);
    }
    if (sbd != NULL && sbd->shade_colors != NULL) {
        free(sbd->shade_colors);
    }
    if (sbd != NULL) {
        free(sbd);
    }
    sbd = NULL;
}

static void mgos_intern_symmetric_bar_loop(mgos_rgbleds* leds)
{
    uint32_t num_rows = leds->panel_height;
    uint32_t num_cols = leds->panel_width;
    int internal_pix_pos = 0;

    tools_rgb_data* used_colors;
    tools_rgb_data out_pix;
    int run = 1;
    run = run <= 0 ? 0 : run;

    if (sbd->atd->is_noisy) {
        leds->pix_pos = 0;
    } else {
        sbd->atd->fade = 1.0;
    }

    internal_pix_pos = (leds->pix_pos > sbd->mid_pos_cut) ? (num_rows - 1 - leds->pix_pos) : leds->pix_pos;
    int range = ((int)round(sbd->atd->level * sbd->mid_pos_cut)) + internal_pix_pos;
    LOG(LL_VERBOSE_DEBUG, ("led_effects_symmetric_bar:\tLevel: %.02f\tAverage: %.02f\tpix_pos: %d\trange: %d\tfade: %.02f", sbd->atd->level, sbd->atd->level_average, internal_pix_pos, range, sbd->atd->fade));
    while (run--) {
        int mid_upper = (int)ceil(sbd->mid_pos);
        int mid_lower = (int)floor(sbd->mid_pos);

        switch (mgos_sys_config_get_ledeffects_symmetric_bar_colormode()) {
        case 1:
            used_colors = sbd->all_colors;
            break;
        case 0:
        default:
            used_colors = sbd->shade_colors;
            break;
        }
        mgos_universal_led_set_all(leds, sbd->back_ground);

        int start_row = min(mid_upper, num_rows);
        int end_row = min((mid_upper + range), num_rows);
        for (int row = start_row; row < end_row; row++) {
            out_pix = tools_fade_color(used_colors[row - mid_upper], sbd->atd->fade);
            for (int col = 0; col < num_cols; col++) {
                LOG(LL_VERBOSE_DEBUG, ("ledeffects_symmetric_bar:\tR: 0x%.02X\tG: 0x%.02X\tB: 0x%.02X", out_pix.r, out_pix.g, out_pix.b));
                mgos_universal_led_plot_pixel(leds, col, num_rows - 1 - row, out_pix, false);
            }
        }

        start_row = max(mid_lower - range, 0);
        end_row = min(mid_lower, num_rows);
        for (int row = start_row; row < end_row; row++) {
            out_pix = tools_fade_color(used_colors[end_row - row], sbd->atd->fade);
            for (int col = 0; col < num_cols; col++) {
                LOG(LL_VERBOSE_DEBUG, ("ledeffects_symmetric_bar:\tR: 0x%.02X\tG: 0x%.02X\tB: 0x%.02X", out_pix.r, out_pix.g, out_pix.b));
                mgos_universal_led_plot_pixel(leds, col, num_rows - 1 - row, out_pix, false);
            }
        }

        mgos_universal_led_show(leds);
        if (!sbd->atd->is_noisy) {
            if (leds->pix_pos == sbd->mid_pos_cut) {
                mgos_msleep(mgos_sys_config_get_ledeffects_symmetric_bar_sleep());
            }
            leds->pix_pos = (leds->pix_pos + 1) % leds->panel_height;
        }
    }
}

void mgos_ledeffects_symmetric_bar(void* param, mgos_rgbleds_action action)
{
    static uint32_t max_time = 0;
    uint32_t time = (mgos_uptime_micros() / 1000);
    mgos_rgbleds* leds = (mgos_rgbleds*)param;

    switch (action) {
    case MGOS_RGBLEDS_ACT_INIT:
        LOG(LL_INFO, ("mgos_ledeffects_symmetric_bar: called (init)"));
        mgos_intern_symmetric_bar_init(leds);    
        break;
    case MGOS_RGBLEDS_ACT_EXIT:
        LOG(LL_INFO, ("mgos_ledeffects_symmetric_bar: called (exit)"));
        mgos_intern_symmetric_bar_exit(leds);    
        break;
    case MGOS_RGBLEDS_ACT_LOOP:
        mgos_intern_symmetric_bar_loop(leds);
        if (do_time) {
            time = (mgos_uptime_micros() /1000) - time;
            max_time = (time > max_time) ? time : max_time;
            LOG(LL_DEBUG, ("Symmetric bar loop duration: %d milliseconds, max: %d ...", time / 1000, max_time / 1000));
        }
        break;
    }
}

bool mgos_symmetric_bar_init(void) {
  LOG(LL_INFO, ("mgos_symmetric_bar_init ..."));
  ledmaster_add_effect("ANIM_SYMMETRIC_BAR", mgos_ledeffects_symmetric_bar);
  return true;
}
