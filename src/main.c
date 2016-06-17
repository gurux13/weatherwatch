#include <pebble.h>




//variables
static Window *s_main_window;
static TextLayer *s_time_layer;
static TextLayer *s_date_layer;
static TextLayer *s_weather_layer;
static TextLayer *s_battery_layer;
static GFont s_rufont_18;
static GFont s_rufont_14;
static char weather_info [64] = {0};
static bool s_connected = false;
static int s_battery = 0;
static bool s_battery_charging = false;

static char s_date_buffer [16] = {0};
static char s_battery_buffer [4] = {0};
static char s_time_buffer [8] = {0};
static char s_weather_buffer [64] = {0};
static unsigned s_last_weather_at = 0;
#define STORAGE_KEY_WEATHER 0
#define STORAGE_KEY_WEATHER_AGE 1


//Events
static void draw_status() {
  
  if (!s_connected)
    strcpy(s_battery_buffer, "NC");
  else 
    snprintf(s_battery_buffer, sizeof(s_battery_buffer), "%d", s_battery);
  text_layer_set_text(s_battery_layer, s_battery_buffer);
}

static void battery_state_handler(BatteryChargeState state) {
  // Report the current charge percentage
  s_battery = state.charge_percent;
  s_battery_charging = state.is_charging;
  draw_status();
}

static void subscribe_battery() {
  battery_state_service_subscribe(battery_state_handler);
}


//Events - connection



static void app_connection_handler(bool connected) {
  if (!connected)
    vibes_double_pulse();
  s_connected = connected;
  draw_status();
}

static void kit_connection_handler(bool connected) {
  APP_LOG(APP_LOG_LEVEL_INFO, "PebbleKit %sconnected", connected ? "" : "dis"); 
}
static void subscribe_connection() {
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = app_connection_handler,
    .pebblekit_connection_handler = kit_connection_handler
  });
}
//Events - init
static void read_event_values() {
  bool app_connection = connection_service_peek_pebble_app_connection();
  bool kit_connection = connection_service_peek_pebblekit_connection();
  s_connected = app_connection | kit_connection;
  BatteryChargeState state = battery_state_service_peek();
  s_battery = state.charge_percent;
  s_battery_charging = state.is_charging;
}

static void subscribe_events() {
  subscribe_battery();
  subscribe_connection();
}
//Weather

static void load_weather()
{
  if (persist_exists(STORAGE_KEY_WEATHER)){
    persist_read_string(STORAGE_KEY_WEATHER, weather_info, sizeof(weather_info));
  }
  if (persist_exists(STORAGE_KEY_WEATHER_AGE)){
    int val;
    val = persist_read_int(STORAGE_KEY_WEATHER_AGE);
    s_last_weather_at = *(unsigned*)(&val); //Convert int to unsigned
  }
  char obtained_at [32];
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  strftime(obtained_at, sizeof(obtained_at), "%d.%m.%y %H:%M", tick_time);
  APP_LOG(APP_LOG_LEVEL_INFO, "Loaded weather info from persist: %s, obtained at %s", weather_info, obtained_at);
}
static void save_weather() {
  if (weather_info[0]) {
    persist_write_string(STORAGE_KEY_WEATHER, weather_info);
    int val;
    val = *(int*)(&s_last_weather_at);
    persist_write_int(STORAGE_KEY_WEATHER_AGE, val);
  }
}
static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  // Store incoming information
  static char temperature_buffer[8];
  static char conditions_buffer[32];
  static char wind_buffer[32];

  // Read tuples for data
  Tuple *temp_tuple = dict_find(iterator, MESSAGE_KEY_KEY_TEMPERATURE);
  Tuple *conditions_tuple = dict_find(iterator, MESSAGE_KEY_KEY_CONDITIONS);
  Tuple *wind_tuple = dict_find(iterator, MESSAGE_KEY_KEY_WIND);

  // If all data is available, use it
  if(temp_tuple && conditions_tuple && wind_tuple) {
    snprintf(temperature_buffer, sizeof(temperature_buffer), "%s", temp_tuple->value->cstring);
    snprintf(conditions_buffer, sizeof(conditions_buffer), "%s", conditions_tuple->value->cstring);
    snprintf(wind_buffer, sizeof(wind_buffer), "%s", wind_tuple->value->cstring);

    // Assemble full string and display
    snprintf(s_weather_buffer, sizeof(s_weather_buffer), "%s, %s, %s", temperature_buffer, wind_buffer, conditions_buffer);
    strcpy(weather_info, s_weather_buffer);
    text_layer_set_text(s_weather_layer, s_weather_buffer);
    s_last_weather_at = time(NULL);
    save_weather();
  }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}
static void request_weather();
static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed (%d)!", reason);
  
  if (!weather_info[0]) //Have no weather at all.
    request_weather();
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}
static void request_weather() {
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);

  // Add a key-value pair
  dict_write_uint8(iter, 0, 0);

  // Send the message!
  app_message_outbox_send();
}


static const char * weekday(int day) {
  switch (day) {
    case 0:
      return "Вс";
    case 1:
      return "Пн";
    case 2:
      return "Вт";
    case 3:
      return "Ср";
    case 4:
      return "Чт";
    case 5:
      return "Пт";
    case 6:
      return "Сб";
    default:
      return "--";
  }
}

//Time

static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);

  // Write the current hours and minutes into a buffer
  
  strftime(s_time_buffer, sizeof(s_time_buffer), clock_is_24h_style() ?
                                          "%H:%M" : "%I:%M", tick_time);
  
  text_layer_set_text(s_time_layer, s_time_buffer);
  // Display this time on the TextLayer
  
  
  strftime(s_date_buffer, sizeof(s_date_buffer), "%d.%m.%y", tick_time);
  strcat(s_date_buffer, " ");
  strcat(s_date_buffer, weekday(tick_time->tm_wday));
  text_layer_set_text(s_date_layer, s_date_buffer);
  
}
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
  if(tick_time->tm_min == 0) {
    vibes_short_pulse();
  }
  bool need_weather = false;
  need_weather |= tick_time->tm_min % 30 == 0;
  need_weather |= weather_info[0] == 0;
  need_weather |= (time(NULL) - s_last_weather_at) / (30*60) > 0;
  if(need_weather) {
    
    request_weather();
  }
}

//Window

static void set_text_prop(TextLayer * layer, const char * font, GTextAlignment align, bool white) {
  if (white) {
    text_layer_set_background_color(layer, GColorWhite);
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

static void main_window_load(Window *window) {
  APP_LOG(APP_LOG_LEVEL_INFO, "main_window_load()");
  // Get information about the Window
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Create the TextLayer with specific bounds
  int y = 0;
  int h = 18;
  s_date_layer = text_layer_create(
      GRect(0, y, bounds.size.w, h));
  s_battery_layer = text_layer_create(
      GRect(120, y, bounds.size.w - 120, h));
  y += h;
  h = 49;
  s_time_layer = text_layer_create(
      GRect(0, y-5, bounds.size.w, h));
  y += h - 5;
  h = 40;
  s_weather_layer = text_layer_create(
      GRect(0, y, bounds.size.w, h));
  y += h;
  
  // Improve the layout to be more like a watchface
  set_text_prop(s_date_layer, NULL, GTextAlignmentLeft, false);
  set_text_prop(s_battery_layer, FONT_KEY_GOTHIC_18_BOLD, GTextAlignmentRight, false);
  set_text_prop(s_time_layer, FONT_KEY_LECO_42_NUMBERS, GTextAlignmentCenter, false);
  set_text_prop(s_weather_layer, NULL, GTextAlignmentCenter, true);
  
  // Create GFont
  s_rufont_18 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_RUFONT_18));
  s_rufont_14 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_RUFONT_14));

// Apply to TextLayer
  text_layer_set_font(s_date_layer, s_rufont_18);
  text_layer_set_font(s_weather_layer, s_rufont_18);
  //text_layer_set_font(s_battery_layer, s_rufont_14);

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_weather_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_battery_layer));
  update_time();
  load_weather();
  if (weather_info[0] == 0) {
    request_weather();
  }
  else {
    text_layer_set_text(s_weather_layer, weather_info);
  }
  read_event_values();
  draw_status();
  
}

static void main_window_unload(Window *window) {
  APP_LOG(APP_LOG_LEVEL_INFO, "main_window_unload()");
  // Destroy TextLayer
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_date_layer);
  text_layer_destroy(s_weather_layer);
  text_layer_destroy(s_battery_layer);
  fonts_unload_custom_font(s_rufont_18);
  fonts_unload_custom_font(s_rufont_14);
}

//System
static void init() {
  APP_LOG(APP_LOG_LEVEL_INFO, "init()");
  
  // Create main Window element and assign to pointer
  s_main_window = window_create();

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);

  // Make sure the time is displayed from the start


  // Register with TickTimerService
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
    // Register callbacks
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);

  // Open AppMessage
  //app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
  app_message_open(128, 32);
  subscribe_events();
}

static void deinit() {
  // Destroy Window
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}