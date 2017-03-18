#include <pebble.h>
#include "commons.h"

void set_text_prop(TextLayer * layer, const char * font, GTextAlignment align, bool white) {
  if (white) {
    text_layer_set_background_color(layer, GColorClear);
    text_layer_set_text_color(layer, GColorBlack);
  }
  else {
    text_layer_set_background_color(layer, GColorBlack);
    text_layer_set_text_color(layer, GColorWhite);
    }
  if (font)
    text_layer_set_font(layer, fonts_get_system_font(font));
  text_layer_set_text_alignment(layer, align);
}

char * cut_till(char ** line, char delimiter) {
  char * ptr = *line;
  while (**line && **line != delimiter) ++*line;
  if (**line) {
    **line = 0;
    ++*line;
  }
  return ptr;
}