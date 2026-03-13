#pragma once
#include <lvgl.h>

// ---------------------------------------------------------------------------
// Pinchy the crab — drawn from LVGL primitives, fully animated
// ---------------------------------------------------------------------------

static struct {
  lv_obj_t *cont;
  lv_obj_t *body;
  lv_obj_t *eye_l, *eye_r;
  lv_obj_t *pupil_l, *pupil_r;
  lv_obj_t *claw_l, *claw_r;
  lv_obj_t *tip_l, *tip_r;
  int       base_x;
  lv_anim_t idle_anim;
} crab;

static void _crab_set_x(void *obj, int32_t v) { lv_obj_set_x((lv_obj_t*)obj, v); }
static void _crab_set_y(void *obj, int32_t v) { lv_obj_set_y((lv_obj_t*)obj, v); }

void crab_init(lv_obj_t *parent, int x, int y) {
  crab.base_x = x;

  // Container
  crab.cont = lv_obj_create(parent);
  lv_obj_remove_style_all(crab.cont);
  lv_obj_set_size(crab.cont, 90, 52);
  lv_obj_set_pos(crab.cont, x, y);
  lv_obj_clear_flag(crab.cont, LV_OBJ_FLAG_SCROLLABLE);

  // Body
  crab.body = lv_obj_create(crab.cont);
  lv_obj_remove_style_all(crab.body);
  lv_obj_set_size(crab.body, 54, 30);
  lv_obj_align(crab.body, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_color(crab.body, lv_color_hex(0xd94f2a), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(crab.body, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(crab.body, 12, LV_PART_MAIN);

  // Eyes (stalks: small vertical rect, white ball on top)
  lv_color_t eye_white = lv_color_hex(0xf0ede8);
  lv_color_t pupil_col = lv_color_hex(0x1a1a1a);

  crab.eye_l = lv_obj_create(crab.cont);
  lv_obj_remove_style_all(crab.eye_l);
  lv_obj_set_size(crab.eye_l, 12, 12);
  lv_obj_set_pos(crab.eye_l, 14, 4);
  lv_obj_set_style_bg_color(crab.eye_l, eye_white, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(crab.eye_l, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(crab.eye_l, LV_RADIUS_CIRCLE, LV_PART_MAIN);

  crab.eye_r = lv_obj_create(crab.cont);
  lv_obj_remove_style_all(crab.eye_r);
  lv_obj_set_size(crab.eye_r, 12, 12);
  lv_obj_set_pos(crab.eye_r, 64, 4);
  lv_obj_set_style_bg_color(crab.eye_r, eye_white, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(crab.eye_r, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(crab.eye_r, LV_RADIUS_CIRCLE, LV_PART_MAIN);

  crab.pupil_l = lv_obj_create(crab.eye_l);
  lv_obj_remove_style_all(crab.pupil_l);
  lv_obj_set_size(crab.pupil_l, 5, 5);
  lv_obj_align(crab.pupil_l, LV_ALIGN_CENTER, 1, 1);
  lv_obj_set_style_bg_color(crab.pupil_l, pupil_col, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(crab.pupil_l, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(crab.pupil_l, LV_RADIUS_CIRCLE, LV_PART_MAIN);

  crab.pupil_r = lv_obj_create(crab.eye_r);
  lv_obj_remove_style_all(crab.pupil_r);
  lv_obj_set_size(crab.pupil_r, 5, 5);
  lv_obj_align(crab.pupil_r, LV_ALIGN_CENTER, 1, 1);
  lv_obj_set_style_bg_color(crab.pupil_r, pupil_col, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(crab.pupil_r, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(crab.pupil_r, LV_RADIUS_CIRCLE, LV_PART_MAIN);

  // Claws
  lv_color_t claw_col = lv_color_hex(0xb83d1e);

  crab.claw_l = lv_obj_create(crab.cont);
  lv_obj_remove_style_all(crab.claw_l);
  lv_obj_set_size(crab.claw_l, 18, 12);
  lv_obj_set_pos(crab.claw_l, 0, 22);
  lv_obj_set_style_bg_color(crab.claw_l, claw_col, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(crab.claw_l, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(crab.claw_l, 4, LV_PART_MAIN);

  crab.claw_r = lv_obj_create(crab.cont);
  lv_obj_remove_style_all(crab.claw_r);
  lv_obj_set_size(crab.claw_r, 18, 12);
  lv_obj_set_pos(crab.claw_r, 72, 22);
  lv_obj_set_style_bg_color(crab.claw_r, claw_col, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(crab.claw_r, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(crab.claw_r, 4, LV_PART_MAIN);
}

void crab_idle() {
  lv_anim_del(crab.cont, (lv_anim_exec_xcb_t)_crab_set_x);
  lv_anim_del(crab.cont, (lv_anim_exec_xcb_t)_crab_set_y);

  lv_anim_init(&crab.idle_anim);
  lv_anim_set_exec_cb(&crab.idle_anim, (lv_anim_exec_xcb_t)_crab_set_x);
  lv_anim_set_var(&crab.idle_anim, crab.cont);
  lv_anim_set_values(&crab.idle_anim, crab.base_x - 4, crab.base_x + 4);
  lv_anim_set_time(&crab.idle_anim, 900);
  lv_anim_set_playback_time(&crab.idle_anim, 900);
  lv_anim_set_repeat_count(&crab.idle_anim, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(&crab.idle_anim, lv_anim_path_ease_in_out);
  lv_anim_start(&crab.idle_anim);
}

void crab_excited() {
  lv_anim_del(crab.cont, (lv_anim_exec_xcb_t)_crab_set_x);
  lv_anim_del(crab.cont, (lv_anim_exec_xcb_t)_crab_set_y);

  // Happy bounce — claws grow then shrink
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)_crab_set_y);
  lv_anim_set_var(&a, crab.cont);
  int cy = lv_obj_get_y(crab.cont);
  lv_anim_set_values(&a, cy, cy - 10);
  lv_anim_set_time(&a, 180);
  lv_anim_set_playback_time(&a, 180);
  lv_anim_set_repeat_count(&a, 2);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_start(&a);
}

void crab_processing() {
  lv_anim_del(crab.cont, (lv_anim_exec_xcb_t)_crab_set_x);
  lv_anim_del(crab.cont, (lv_anim_exec_xcb_t)_crab_set_y);

  // Rapid side-to-side scuttle
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)_crab_set_x);
  lv_anim_set_var(&a, crab.cont);
  lv_anim_set_values(&a, crab.base_x - 8, crab.base_x + 8);
  lv_anim_set_time(&a, 200);
  lv_anim_set_playback_time(&a, 200);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(&a, lv_anim_path_linear);
  lv_anim_start(&a);
}

void crab_error() {
  lv_anim_del(crab.cont, (lv_anim_exec_xcb_t)_crab_set_x);
  lv_anim_del(crab.cont, (lv_anim_exec_xcb_t)_crab_set_y);

  // Confused jitter
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)_crab_set_x);
  lv_anim_set_var(&a, crab.cont);
  lv_anim_set_values(&a, crab.base_x - 5, crab.base_x + 5);
  lv_anim_set_time(&a, 80);
  lv_anim_set_playback_time(&a, 80);
  lv_anim_set_repeat_count(&a, 4);
  lv_anim_set_path_cb(&a, lv_anim_path_linear);
  lv_anim_start(&a);
}
