#include <pebble.h>
#include "src/c/pressure.h"
#include "src/c/commons.h"
#include "src/c/forecast_days.h"


//variables
static Window *s_main_window;
static TextLayer *s_time_layer;
static TextLayer *s_date_layer;
static TextLayer *s_weather_layer;
static TextLayer *s_forecast_layer;
static TextLayer *s_lastupdate_layer;
static TextLayer *s_battery_layer;
static TextLayer *s_traffic_layer;
static TextLayer *s_currency_layer;
static BitmapLayer * s_traffic_image_layer;
static BitmapLayer * s_weather_conditions_image_layer;
static BitmapLayer * s_weather_conditions2_image_layer;
static BitmapLayer * s_weather_conditions3_image_layer;
//static Layer * s_pressure_plot_layer;

static GFont s_rufont_18;
static GFont s_rufont_14;
static char weather_info [64] = {0};
static bool s_connected = false;
static int s_battery = 0;
static bool s_battery_charging = false;

static char s_date_buffer [16] = {0};
static char s_battery_buffer [8] = {0};
static char s_time_buffer [8] = {0};
static char s_weather_buffer [64] = {0};
static char s_forecast_buffer [128] = {0};
static char s_forecast_days_buffer [128] = {0};
static char s_traffic_buffer [4] = "y-";
static char s_currency_buffer [32] = {0};
static char s_lastupdate_buffer [32] = {0};
static long s_last_weather_at = 0;
#define STORAGE_KEY_WEATHER 0
#define STORAGE_KEY_WEATHER_AGE 1
#define STORAGE_KEY_WEATHER_FORECAST 2
#define STORAGE_KEY_TRAFFIC 3
#define STORAGE_KEY_CURRENCY 4
#define STORAGE_KEY_WEATHER_FORECAST_DAYS 5


static void draw_weather();


//Health
static void update_health() {
  HealthMetric metric = HealthMetricStepCount;
  time_t start = time_start_of_today();
  time_t end = time(NULL);
  
  // Check the metric has data available for today
  HealthServiceAccessibilityMask mask = health_service_metric_accessible(metric, 
    start, end);
  
  if(mask & HealthServiceAccessibilityMaskAvailable) {
    // Data is available!
  } else {
    // No data recorded yet today
  }
}
static void health_handler(HealthEventType event, void *context) {
  // Which type of event occurred?
  switch(event) {
    case HealthEventSignificantUpdate:
      update_health();
      break;
    case HealthEventMovementUpdate:
      update_health();
      break;
    case HealthEventSleepUpdate:
      update_health();
      break;
    default:
      update_health();
      break;
  }
}
static void subscribe_health() {
  if(!health_service_events_subscribe(health_handler, NULL)) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Health not available!");
  }
}
static void lastupdate_string(char * buffer, int bufsize, int *col) {
  long now = time(NULL);
  long age = now - s_last_weather_at;
  if (age > 60*60) *col = 3;
  else if (age > 30*60) *col = 2;
  else *col = 1;
  struct tm *tick_time = localtime(&s_last_weather_at);
  strftime(buffer, bufsize, "%H:%M", tick_time);
}
static void draw_lastupdate() {
  int col;
  lastupdate_string(s_lastupdate_buffer, sizeof(s_lastupdate_buffer), &col);
  text_layer_set_text(s_battery_layer, s_battery_buffer);
  if (col == 3) {
    text_layer_set_background_color(s_lastupdate_layer, GColorSunsetOrange);
  } else if (col == 2) {
    text_layer_set_background_color(s_lastupdate_layer, GColorYellow);
  } else {
    text_layer_set_background_color(s_lastupdate_layer, GColorBrightGreen);
  }
  
  text_layer_set_text(s_lastupdate_layer, s_lastupdate_buffer);
}

//Events
static void draw_status() {
  
  if (!s_connected)
    strcpy(s_battery_buffer, "NC");
  else 
    snprintf(s_battery_buffer, sizeof(s_battery_buffer), "%d", s_battery);
  if (s_battery_charging && strlen(s_battery_buffer) + 1 < sizeof(s_battery_buffer)) {
    strcat(s_battery_buffer, "+");
  }
  text_layer_set_text(s_battery_layer, s_battery_buffer);
  if (s_battery <= 20) {
    text_layer_set_background_color(s_time_layer, GColorSunsetOrange);
  } else if (s_battery <= 40) {
    text_layer_set_background_color(s_time_layer, GColorYellow);
  } else {
    text_layer_set_background_color(s_time_layer, GColorBrightGreen);
  }

}

static void battery_state_handler(BatteryChargeState state) {
  // Report the current charge percentage
  s_battery = state.charge_percent;
  s_battery_charging = state.is_charging;
  draw_status();
  draw_lastupdate();
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
  subscribe_health();
}
//Weather

static void load_data()
{
  if (persist_exists(STORAGE_KEY_WEATHER)){
    persist_read_string(STORAGE_KEY_WEATHER, weather_info, sizeof(weather_info));
  }
  if (persist_exists(STORAGE_KEY_WEATHER_FORECAST)){
    persist_read_string(STORAGE_KEY_WEATHER_FORECAST, s_forecast_buffer, sizeof(s_forecast_buffer));
  }
    if (persist_exists(STORAGE_KEY_WEATHER_FORECAST_DAYS)){
    persist_read_string(STORAGE_KEY_WEATHER_FORECAST_DAYS, s_forecast_days_buffer, sizeof(s_forecast_days_buffer));
  }
  if (persist_exists(STORAGE_KEY_WEATHER_AGE)){
    persist_read_data(STORAGE_KEY_WEATHER_AGE, &s_last_weather_at, sizeof(s_last_weather_at));
    if (s_last_weather_at > time(NULL)) {
      s_last_weather_at = 0; //Something wrong
    }
    draw_lastupdate();
  }
  if (persist_exists(STORAGE_KEY_TRAFFIC)){
    persist_read_string(STORAGE_KEY_TRAFFIC, s_traffic_buffer, sizeof(s_traffic_buffer));
  }
  if (persist_exists(STORAGE_KEY_CURRENCY)){
    persist_read_string(STORAGE_KEY_CURRENCY, s_currency_buffer, sizeof(s_currency_buffer));
  }
  if (*weather_info) {
    draw_weather();
  }
  char obtained_at [32];
  time_t temp = s_last_weather_at;
  struct tm *tick_time = localtime(&temp);
  strftime(obtained_at, sizeof(obtained_at), "%d.%m.%y %H:%M", tick_time);
  APP_LOG(APP_LOG_LEVEL_INFO, "Weather info from persist: %s", weather_info);
  APP_LOG(APP_LOG_LEVEL_INFO, "Days forecast from persist: %s", s_forecast_days_buffer);
  APP_LOG(APP_LOG_LEVEL_INFO, "Obtained at %s", obtained_at);
}

static void save_data() {
  if (weather_info[0]) {
    persist_write_string(STORAGE_KEY_WEATHER, weather_info);
    persist_write_data(STORAGE_KEY_WEATHER_AGE, &s_last_weather_at, sizeof(s_last_weather_at));
    persist_write_string(STORAGE_KEY_WEATHER_FORECAST, s_forecast_buffer);
    
    persist_write_string(STORAGE_KEY_WEATHER_FORECAST_DAYS, s_forecast_days_buffer);
  }
  if (s_traffic_buffer[0]) {
    persist_write_string(STORAGE_KEY_TRAFFIC, s_traffic_buffer);
  }
  if (s_currency_buffer[0]) {
    persist_write_string(STORAGE_KEY_CURRENCY, s_currency_buffer);
  }
}
static void draw_traffic() {
  char color = s_traffic_buffer[0];
  char * traffic_text = s_traffic_buffer + 1;
  static GBitmap *s_traffic_bitmap = NULL;
  if (s_traffic_bitmap != NULL) {
    gbitmap_destroy(s_traffic_bitmap);
  }
  APP_LOG(APP_LOG_LEVEL_INFO, "Drawing traffic: %s", s_traffic_buffer);
  int resource = 0;
  GColor colorCode = GColorBlack;
  switch (color) {
    case 'g':
      resource = RESOURCE_ID_IMAGE_TRAFFIC_GREEN;
      break;
    case 'y':
      resource = RESOURCE_ID_IMAGE_TRAFFIC_YELLOW;
      break;
    default:
      resource = RESOURCE_ID_IMAGE_TRAFFIC_RED;
      colorCode = GColorWhite;
      break;
  }
  //TODO: Remove me
  s_traffic_bitmap = gbitmap_create_with_resource(resource);
  bitmap_layer_set_bitmap(s_traffic_image_layer, s_traffic_bitmap);
  text_layer_set_text(s_traffic_layer, traffic_text);
  text_layer_set_text_color(s_traffic_layer, colorCode);
}
static void draw_currency() {
  text_layer_set_text(s_currency_layer, s_currency_buffer);
}

void draw_weather_icon(char * icon) {
  static GBitmap *s_icon_bitmap = NULL;
  static GBitmap *s_icon_bitmap2 = NULL;
  static GBitmap *s_icon_bitmap3 = NULL;
  if (s_icon_bitmap != NULL) {
    gbitmap_destroy(s_icon_bitmap);
  }
    if (s_icon_bitmap2 != NULL) {
    gbitmap_destroy(s_icon_bitmap2);
  }
  if (s_icon_bitmap3 != NULL) {
    gbitmap_destroy(s_icon_bitmap3);
  }
  APP_LOG(APP_LOG_LEVEL_INFO, "Drawing weather: %c", icon);
  int resource = get_conditions_icon_id(icon[0]);
  s_icon_bitmap = gbitmap_create_with_resource(resource);
  bitmap_layer_set_bitmap(s_weather_conditions_image_layer, s_icon_bitmap);
  if (!icon[1])
    return;
  resource = get_conditions_icon_id(icon[1]);
  s_icon_bitmap2 = gbitmap_create_with_resource(resource);
  bitmap_layer_set_bitmap(s_weather_conditions2_image_layer, s_icon_bitmap2);
  if (!icon[2])
    return;
  resource = get_conditions_icon_id(icon[2]);
  s_icon_bitmap3 = gbitmap_create_with_resource(resource);
  bitmap_layer_set_bitmap(s_weather_conditions3_image_layer, s_icon_bitmap3);
}

static void draw_weather() {
  char * cutme = weather_info;
  char * weather = cut_till(&cutme, '\n');
  if (*cutme) {
    char* icon = cutme;
    strncpy(s_weather_buffer, weather, sizeof(s_weather_buffer));
    *(cutme - 1) = '\n'; //restore '\n'
    draw_weather_icon(icon);
    text_layer_set_text(s_weather_layer, s_weather_buffer);
  }
}
static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  // Store incoming information
  
  APP_LOG(APP_LOG_LEVEL_INFO, "Received something from phone");
  // Read tuples for data
  Tuple *weather_tuple = dict_find(iterator, MESSAGE_KEY_KEY_WEATHER);
  Tuple *forecast_tuple = dict_find(iterator, MESSAGE_KEY_KEY_FORECAST);
  Tuple *traffic_tuple = dict_find(iterator, MESSAGE_KEY_KEY_TRAFFIC);
  Tuple *currency_tuple = dict_find(iterator, MESSAGE_KEY_KEY_CURRENCY);
  Tuple *pressure_tuple = dict_find(iterator, MESSAGE_KEY_KEY_PRESSURE);
  // If all data is available, use it
  if(weather_tuple && forecast_tuple) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Received weather");
    
    char * fcast = forecast_tuple->value->cstring;
    strncpy(s_forecast_buffer, cut_till(&fcast, '|'), sizeof(s_forecast_buffer));
    strncpy(s_forecast_days_buffer, fcast, sizeof(s_forecast_buffer));
    strncpy(s_weather_buffer, weather_tuple->value->cstring, sizeof(s_weather_buffer));
    APP_LOG(APP_LOG_LEVEL_INFO, "Days forecast: %s", s_forecast_days_buffer);
    on_forecast_received(s_forecast_days_buffer);
    // Assemble full string and display
    APP_LOG(APP_LOG_LEVEL_INFO, "w:%s, f:%s", s_weather_buffer, s_forecast_buffer);
    strcpy(weather_info, s_weather_buffer);
    draw_weather();
    text_layer_set_text(s_forecast_layer, s_forecast_buffer);
    
    s_last_weather_at = time(NULL);
    save_data();
    draw_lastupdate();
  }
  if (traffic_tuple) {
    snprintf(s_traffic_buffer, sizeof(s_traffic_buffer), "%s", traffic_tuple->value->cstring);
    draw_traffic();
    save_data();
  }
  if (currency_tuple) {
    snprintf(s_currency_buffer, sizeof(s_currency_buffer), "%s", currency_tuple->value->cstring);
    draw_currency();
    save_data();
  }
  if (pressure_tuple) {
    //on_pressure_received(pressure_tuple->value->int32);
  }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped! Reason: %d", reason);
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
static void vibe_hourly() {
    static uint32_t const segments[] = { 100 };
    VibePattern pat = {
      .durations = segments,
      .num_segments = ARRAY_LENGTH(segments),
    };
    vibes_enqueue_custom_pattern(pat);
}
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
  
  if(tick_time->tm_min == 0) {
    vibe_hourly();
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


static void main_window_load(Window *window) {
  APP_LOG(APP_LOG_LEVEL_INFO, "main_window_load()");
  // Get information about the Window
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Create the TextLayer with specific bounds
  int y = 0;
  int h = 18;
  s_date_layer = text_layer_create(
      GRect(0, y, bounds.size.w, h + 4));
  s_battery_layer = text_layer_create(
      GRect(110, y, bounds.size.w - 110, h));
  y += h;
  h = 45;
  s_time_layer = text_layer_create(
      GRect(0, y - 4, bounds.size.w, h));
  y += h - 4;
  h = 20;
  s_weather_layer = text_layer_create(
      GRect(0, y, bounds.size.w, h));
  y += h;
  
  s_weather_conditions_image_layer = bitmap_layer_create(
    GRect(0, y, 32, h));
  s_weather_conditions2_image_layer = bitmap_layer_create(
    GRect(32, y, 32, h));
  s_weather_conditions3_image_layer = bitmap_layer_create(
    GRect(64, y, 32, h));

  
  y += h;
  
  s_lastupdate_layer = text_layer_create(
      GRect(113, y - 16, bounds.size.w - 113, 16));

  h = 12;
  s_forecast_layer = text_layer_create(
      GRect(0, y, bounds.size.w, h));
  y += h;
  

  /*s_lastupdate_layer = text_layer_create(
      GRect(113, y, bounds.size.w - 113, h));*/
  //y += h;

  h = fday_init(window_layer, 0, y);
  
  
 // h = 31 - 15;
/*  s_pressure_plot_layer = layer_create(
    GRect(0, y, bounds.size.w, h));*/
  y += h;

  
  h = 25;
  s_traffic_layer = text_layer_create(
      GRect(0, y - 5, 25, 25));
  s_traffic_image_layer = bitmap_layer_create(
    GRect(0, y, 25, 25));

  //Move to the right
  
  s_currency_layer = text_layer_create(
      GRect(30, y, bounds.size.w - 30, 25));
  
  y += h;
//  pressure_init(s_pressure_plot_layer);

  // Improve the layout to be more like a watchface
  set_text_prop(s_date_layer, NULL, GTextAlignmentLeft, false);
  set_text_prop(s_battery_layer, FONT_KEY_GOTHIC_18_BOLD, GTextAlignmentRight, false);
  set_text_prop(s_time_layer, FONT_KEY_LECO_42_NUMBERS, GTextAlignmentCenter, true);
  set_text_prop(s_weather_layer, NULL, GTextAlignmentCenter, true);
  set_text_prop(s_forecast_layer, FONT_KEY_GOTHIC_09, GTextAlignmentCenter, false);
  set_text_prop(s_lastupdate_layer, FONT_KEY_GOTHIC_14, GTextAlignmentCenter, true);
  set_text_prop(s_traffic_layer, FONT_KEY_GOTHIC_24_BOLD, GTextAlignmentCenter, true);
  set_text_prop(s_currency_layer, FONT_KEY_GOTHIC_18_BOLD, GTextAlignmentCenter, true);
  text_layer_set_background_color(s_traffic_layer, GColorClear);
  text_layer_set_background_color(s_time_layer, GColorYellow);
  //text_layer_set_background_color(s_forecast_layer, GColorYellow);
  // Create GFont
  s_rufont_18 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_RUFONT_18));
  s_rufont_14 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_RUFONT_14));

// Apply to TextLayer
  text_layer_set_font(s_date_layer, s_rufont_18);
  text_layer_set_font(s_weather_layer, s_rufont_18);
  //text_layer_set_font(s_forecast_layer, s_rufont_14);
  //text_layer_set_font(s_battery_layer, s_rufont_14);

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_battery_layer));
  
  layer_add_child(window_layer, text_layer_get_layer(s_weather_layer));
  layer_add_child(window_layer, bitmap_layer_get_layer(s_weather_conditions_image_layer));
  layer_add_child(window_layer, bitmap_layer_get_layer(s_weather_conditions2_image_layer));
  layer_add_child(window_layer, bitmap_layer_get_layer(s_weather_conditions3_image_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_forecast_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_lastupdate_layer));
  layer_add_child(window_layer, bitmap_layer_get_layer(s_traffic_image_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_traffic_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_currency_layer));
//  layer_add_child(window_layer, s_pressure_plot_layer);
  
  update_time();
  load_data();
  if (weather_info[0] == 0) {
    request_weather();
  }
  else {
    //text_layer_set_text(s_weather_layer, weather_info);
    text_layer_set_text(s_forecast_layer, s_forecast_buffer);
    on_forecast_received(s_forecast_days_buffer);
  }
  read_event_values();
  draw_status();
  draw_traffic();
  draw_currency();
  update_health();
}

static void main_window_unload(Window *window) {
  APP_LOG(APP_LOG_LEVEL_INFO, "main_window_unload()");
  // Destroy TextLayer
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_date_layer);
  text_layer_destroy(s_weather_layer);
  text_layer_destroy(s_forecast_layer);
  text_layer_destroy(s_battery_layer);
  text_layer_destroy(s_traffic_layer);
  text_layer_destroy(s_lastupdate_layer);
  text_layer_destroy(s_currency_layer);
  bitmap_layer_destroy(s_traffic_image_layer);
  bitmap_layer_destroy(s_weather_conditions_image_layer);
  bitmap_layer_destroy(s_weather_conditions2_image_layer);
  bitmap_layer_destroy(s_weather_conditions3_image_layer);
  fonts_unload_custom_font(s_rufont_18);
  fonts_unload_custom_font(s_rufont_14);
  fday_deinit();
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