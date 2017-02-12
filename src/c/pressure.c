#include <pebble.h>
#include "src/c/pressure.h"
#define MAX_MEASUREMENTS 100
#define KEY_PRESSURE_CURSOR 100
#define KEY_PRESSURE_LASTHOUR 101
#define KEY_PRESSURE_DATA 102
static int last_pressure_saved = 0;
static int pressure_list [MAX_MEASUREMENTS];
static int cursor = 0;
static Layer * draw_layer;
static int max_measurements_fits = 0;
#define STROKE_WIDTH 2

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 1);
  int height = layer_get_bounds(layer).size.h - 2;
  int min = 4096;
  int max = 0;
  for (int i = 0; i < max_measurements_fits; ++i) {
    //APP_LOG(APP_LOG_LEVEL_INFO, "CALC: i = %d, i'th = %d, min = %d, max = %d",i, pressure_list[i], min, max);
    if (pressure_list[i] == 0)
      continue;
    if (min > pressure_list[i]) min = pressure_list[i];
    if (max < pressure_list[i]) max = pressure_list[i];
  }
  if (max == min){
    max = min + 1; //Avoid div by zero
    --min;
  }
  //APP_LOG(APP_LOG_LEVEL_INFO, "Max: %d, min: %d", max, min);
  int pos = 0;
  bool first_loop = true;
  int now_val = pressure_list[cursor];
  for (int i = (cursor + 1) % max_measurements_fits; ; ++i, i %= max_measurements_fits) {
    if (!first_loop && i == (cursor + 1) % max_measurements_fits)
      break;
    first_loop = false;
    if (pressure_list[i] == 0)
      continue;
    
    int val = now_val - pressure_list[i];
    int h = height / 2 - val;
    if (h < 0) h = 0;
    if (h > height)
      h = height;
    for (int shift = 0; shift < STROKE_WIDTH; ++shift)
      graphics_draw_line(ctx, GPoint(pos + shift, height - h), GPoint(pos + shift, height));
    //graphics_draw_line(ctx, GPoint(pos, height - h), GPoint(pos, height));
    pos += STROKE_WIDTH;
    //APP_LOG(APP_LOG_LEVEL_INFO, "Cur vals[%d]: item=%d, val=%d, h=%d",i, pressure_list[i], val, h);
  }
  //APP_LOG(APP_LOG_LEVEL_INFO, "Height: %d, measures: %d", height, max_measurements_fits);
  
}
static void cursor_advance() {
  ++cursor;
  cursor %= max_measurements_fits;
}
static void load_pressure() {
  cursor = persist_read_int(KEY_PRESSURE_CURSOR);
  memset(pressure_list, 0, sizeof(pressure_list));
  persist_read_data(KEY_PRESSURE_DATA, pressure_list, sizeof(pressure_list));
  last_pressure_saved = persist_read_int(KEY_PRESSURE_LASTHOUR);
}
static void save_pressure() {
  int last_val = 0;
  for (int i = 0; i < max_measurements_fits; ++i) {
    if (pressure_list[i] != 0) {
      last_val = pressure_list[i];
      break;
    }
  }
  for (int i = 0; i < max_measurements_fits; ++i) {
    if (pressure_list[i] != 0) {
      
      last_val = pressure_list[i];
    }
    else {
      pressure_list[i] = last_val;
    }
  }
  
  persist_write_int(KEY_PRESSURE_CURSOR, cursor);
  persist_write_int(KEY_PRESSURE_LASTHOUR, last_pressure_saved);
  persist_write_data(KEY_PRESSURE_DATA, pressure_list, sizeof(pressure_list));
}
void pressure_init(Layer * layer) {

  draw_layer = layer;
  GRect rect = layer_get_bounds(layer);
  max_measurements_fits = rect.size.w / STROKE_WIDTH;
  
  layer_set_update_proc(layer, canvas_update_proc);
  //APP_LOG(APP_LOG_LEVEL_INFO, "Measurements fit: %d", max_measurements_fits);
  load_pressure();
  save_pressure();
}



void on_pressure_received(int pressure) {
  time_t now = time(NULL);
  int hours = now / 3600;
  if (now % 3600 >= 1800) ++hours;
  if (hours > last_pressure_saved) {
    cursor_advance();
    last_pressure_saved = hours;
  }
  pressure_list[cursor] = pressure;
  save_pressure();
  layer_mark_dirty(draw_layer);
}
