#include <pebble.h>
#include "forecast_days.h"
#include "src/c/commons.h"
#include <pebble-effect-layer/pebble-effect-layer.h>

#define SCREEN_WIDTH 144

#define FCAST_HEIGHT 32
#define FCAST_COUNT 3
#define PIECE_WIDTH (SCREEN_WIDTH / FCAST_COUNT)
typedef struct {
  TextLayer * date;
  TextLayer * temp;
  EffectLayer * inverter;
  BitmapLayer * icon;
  GBitmap * icon_bmp;
  char * date_text;
  char * temp_text;
} fcast_layers;

void init_fcast_piece(int x, int y, fcast_layers * layers, Layer * window, bool inverted) {
  layers->icon = bitmap_layer_create(GRect(x + 15, y, PIECE_WIDTH - 15, 20));
  layer_add_child(window, bitmap_layer_get_layer(layers->icon));
  layers->date = text_layer_create(GRect(x, y, 15, 20));
  set_text_prop(layers->date, FONT_KEY_GOTHIC_14, GTextAlignmentLeft, true);
  layer_add_child(window, text_layer_get_layer(layers->date));
  layers->temp = text_layer_create(GRect(x, y + 16, PIECE_WIDTH - 2, 20));
  set_text_prop(layers->temp, FONT_KEY_GOTHIC_14, GTextAlignmentCenter, true);
  layer_add_child(window, text_layer_get_layer(layers->temp));
  
  
  if (inverted) {
    layers->inverter = effect_layer_create(GRect(x, y , PIECE_WIDTH, FCAST_HEIGHT));
    effect_layer_add_effect(layers->inverter, effect_invert, NULL);
    layer_add_child(window, effect_layer_get_layer(layers->inverter));
  } else {
    layers->inverter = NULL;
  }
  layers->icon_bmp = NULL;
  /*y += 10;
  layers->night = text_layer_create(GRect(x, y, PIECE_WIDTH, FCAST_HEIGHT));
  set_text_prop(layers->night, FONT_KEY_GOTHIC_09, GTextAlignmentCenter, true);
  layer_add_child(window, text_layer_get_layer(layers->night));*/
  
}

void set_icon(fcast_layers * layer, char icon) {
  
  if (layer->icon_bmp) {
    gbitmap_destroy(layer->icon_bmp);
  }
  
  int resource = get_conditions_icon_id(icon);
  layer->icon_bmp = gbitmap_create_with_resource(resource);
  if (layer->icon_bmp)
    bitmap_layer_set_bitmap(layer->icon, layer->icon_bmp);
}

static fcast_layers s_fcast_layers [FCAST_COUNT];

int fday_init(Layer * window, int x, int y) {
  for (int i = 0; i < FCAST_COUNT; ++i) {
    init_fcast_piece(x, y, s_fcast_layers + i, window, i & 1);
    x += PIECE_WIDTH;
  }
  return FCAST_HEIGHT;
}

char * strdup(char * s) {
  char * retval = malloc(strlen(s) + 1);
  strcpy(retval, s);
  return retval;
}

void replace_ptr (char ** to, char * from) {
  if (*to) free(*to);
  *to = strdup(from);
}

void on_forecast_received(char * fcast) {
  //expect like 10$+1/-1$c|11$+2/0$p|12$+3/-1$s
  char * cut_fcast = strdup(fcast);
  char * cut_fcast_saved = cut_fcast;
  for (int i = 0; i < FCAST_COUNT; ++i) { 
    char * my_fcast = cut_till(&cut_fcast, '|');
    if (!my_fcast || !*my_fcast)
      continue;
    fcast_layers * l = s_fcast_layers + i;
    replace_ptr(&l->date_text, cut_till(&my_fcast, '$'));
    replace_ptr(&l->temp_text, cut_till(&my_fcast, '$'));
    text_layer_set_text(l->date, l->date_text);
    text_layer_set_text(l->temp, l->temp_text);
    set_icon(l, my_fcast[0]);
    //text_layer_set_text(l->night, "-20");
  }
  free(cut_fcast_saved);
}
void fday_deinit() {
  for (int i = 0; i < FCAST_COUNT; ++i) { 
    fcast_layers * l = s_fcast_layers + i;
    text_layer_destroy(l->date);
    text_layer_destroy(l->temp);
    bitmap_layer_destroy(l->icon);
    if (l->inverter) {
      effect_layer_destroy(l->inverter);
    }
  }
}