#include <pebble.h>
#define BATTERY_HEIGHT 3

#define WEATHER_UNKNOWN 0
#define WEATHER_CLEAR_DAY 1
#define WEATHER_CLEAR_NIGHT 2
#define WEATHER_PARTLY_CLOUDY_DAY 3
#define WEATHER_PARTLY_CLOUDY_NIGHT 4
#define WEATHER_CLOUDY 5
#define WEATHER_RAIN 6
#define WEATHER_SNOW 7
#define WEATHER_SLEET 8
#define WEATHER_WIND 9
#define WEATHER_FOG 10

static int s_top = 34;
static int s_width = 144;
static int s_height = 160;

static Window *s_main_window;

static TextLayer *s_time_layer;

static TextLayer *s_ampm_layer;

static TextLayer *s_day_layer;

static TextLayer *s_date_layer;

static TextLayer *s_temp_layer;

static TextLayer *s_next_temp_layer;

static TextLayer *s_weather_layer;

static Layer *s_battery_layer;

static GFont s_time_font;

static GFont s_font_small;

static GFont s_font_tiny;

static int s_is_connected = 1;

static int s_battery_level;

static int s_is_charging = 0;

static GColor8 s_battery_color;

static GBitmap *s_battery_charging;

static GBitmap *s_battery_plugged_in;

static BitmapLayer *s_battery_img_layer;

static GBitmap *s_weather_img;

static BitmapLayer *s_weather_img_layer;

static int s_weather = WEATHER_UNKNOWN;

// Store incoming information
static char s_temperature_buffer[8] = "";
static char s_next_temp_buffer[8] = "";
static char s_weather_layer_buffer[61] = "";

int toupper(int c) {
	return c >= 97 && c <= 122 ? c - 32 : c;
}

void str_upper(char str[]) {
	int len = strlen(str);
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "strlen %d", len);
	for(int i = 0; i < len; i++) {
		if(str[i] == '\0')
			break;
		str[i] = toupper(str[i]);
	}
}

static void get_weather() {
  // Get a tm structure
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
	// Begin dictionary
	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);

	// Add a key-value pair
	// Get weather every 30 minutes, update location every 30 minutes between 8:00 and 22:30 or 4 hours
	dict_write_uint8(iter, MESSAGE_KEY_GET_LOCATION, (tick_time->tm_hour >= 8 && tick_time->tm_hour <= 22) || (tick_time->tm_min == 0 && tick_time->tm_hour % 4 == 0) ? 1 : 0);

	// Send the message!
	app_message_outbox_send();
}

static void bt_handler(bool connected) {
	if(connected) {
		if(s_is_connected == -1) {
			get_weather();
		}
		s_is_connected = 1;
	}
	else if(s_is_connected == 1)
		s_is_connected = 0;
}

static char s_ampm_buffer[3] = "";

static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  // Write the current hours and minutes into a buffer
  static char s_buffer[8];
  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ?
					 "%H:%M" : "%I:%M", tick_time);

  // Display this time on the TextLayer
  text_layer_set_text(s_time_layer, s_buffer);
  
  if(!clock_is_24h_style()) {
    char ampm_buffer[3];
    strftime(ampm_buffer, sizeof(ampm_buffer), "%p", tick_time);
    
		if(strcmp(ampm_buffer, s_ampm_buffer) != 0) {
			strcpy(s_ampm_buffer, ampm_buffer);
			text_layer_set_text(s_ampm_layer, s_ampm_buffer);
		}
  }
	else if(strcmp(s_ampm_buffer, "") != 0) {
		strcpy(s_ampm_buffer, "");
		text_layer_set_text(s_ampm_layer, s_ampm_buffer);
	}
}

static void update_date() {
  // Get a tm structure
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
	
	static char s_day_buffer[10];
	//strftime(s_day_buffer, sizeof(s_day_buffer), "%A", tick_time);	// Tuesday
	strftime(s_day_buffer, sizeof(s_day_buffer), "%a", tick_time);	// Tue
	//str_upper(s_day_buffer);

	text_layer_set_text(s_day_layer, s_day_buffer);
	
	static char s_date_buffer[13];
	//strftime(s_date_buffer, sizeof(s_date_buffer), "%B %d", tick_time);	// October 02
	//strftime(s_date_buffer, sizeof(s_date_buffer), "%a %b %d", tick_time);	// Mon Oct 02
	strftime(s_date_buffer, sizeof(s_date_buffer), "%b %d", tick_time);	// Mon Oct 02
	//str_upper(s_date_buffer);

	text_layer_set_text(s_date_layer, s_date_buffer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
	if((units_changed & DAY_UNIT) == DAY_UNIT) {
		update_date();
	}
	if(tick_time->tm_min % 20 == 0 && (s_battery_level > 10 || s_is_charging > 0)) {
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Get weather %d", s_is_connected);
		if(s_is_connected == 0)
			s_is_connected = -1;
		else if(s_is_connected == 1)
			get_weather();
	}
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
	// Read tuples for data
	Tuple *color = dict_find(iterator, MESSAGE_KEY_COLOR_BG);
	
	if(color) {
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "bg %d", (int)color->value->int32);
		window_set_background_color(s_main_window, GColorFromHEX((int)color->value->int32));
		if((color = dict_find(iterator, MESSAGE_KEY_COLOR_TIME)))
			text_layer_set_text_color(s_time_layer, GColorFromHEX((int)color->value->int32));
		if((color = dict_find(iterator, MESSAGE_KEY_COLOR_TEMP)))
			text_layer_set_text_color(s_temp_layer, GColorFromHEX((int)color->value->int32));
		if((color = dict_find(iterator, MESSAGE_KEY_COLOR_NEXT_TEMP)))
			text_layer_set_text_color(s_next_temp_layer, GColorFromHEX((int)color->value->int32));
		if((color = dict_find(iterator, MESSAGE_KEY_COLOR_DAY)))
			text_layer_set_text_color(s_day_layer, GColorFromHEX((int)color->value->int32));
		if((color = dict_find(iterator, MESSAGE_KEY_COLOR_DATE)))
			text_layer_set_text_color(s_date_layer, GColorFromHEX((int)color->value->int32));
		if((color = dict_find(iterator, MESSAGE_KEY_COLOR_AMPM)))
			text_layer_set_text_color(s_ampm_layer, GColorFromHEX((int)color->value->int32));
		if((color = dict_find(iterator, MESSAGE_KEY_COLOR_WEATHER)))
			text_layer_set_text_color(s_weather_layer, GColorFromHEX((int)color->value->int32));
		if((color = dict_find(iterator, MESSAGE_KEY_COLOR_BATTERY))) {
			s_battery_color = GColorFromHEX((int)color->value->int32);
			layer_mark_dirty(s_battery_layer);
		}
		//set_layer_color(s_)
		return;
	}
	Tuple *temp_tuple = dict_find(iterator, MESSAGE_KEY_TEMPERATURE);
	Tuple *conditions_tuple = dict_find(iterator, MESSAGE_KEY_CONDITIONS);
	
	if(temp_tuple) {
		char temperature_buffer[8];
		
  	snprintf(temperature_buffer, sizeof(temperature_buffer), "%d°", (int)temp_tuple->value->int32);
		if(strcmp(temperature_buffer, s_temperature_buffer) != 0) {
			strcpy(s_temperature_buffer, temperature_buffer);
			text_layer_set_text(s_temp_layer, s_temperature_buffer);
		}
	
		if((temp_tuple = dict_find(iterator, MESSAGE_KEY_NEXT_TEMP))) {
			snprintf(temperature_buffer, sizeof(temperature_buffer), "%d°", (int)temp_tuple->value->int32);
		}
		else
			strcpy(temperature_buffer, "");
		if(strcmp(temperature_buffer, s_next_temp_buffer) != 0) {
			strcpy(s_next_temp_buffer, temperature_buffer);
			text_layer_set_text(s_next_temp_layer, s_next_temp_buffer);
		}
	}

	// If all data is available, use it
	if(conditions_tuple) {
		char weather_layer_buffer[61];
		
  	snprintf(weather_layer_buffer, sizeof(weather_layer_buffer), "%s", conditions_tuple->value->cstring);
		
		// Display
		if(strcmp(weather_layer_buffer, s_weather_layer_buffer) != 0) {
			GSize calcSize = graphics_text_layout_get_content_size(weather_layer_buffer, s_font_tiny,
					GRect(0, 0, s_width - 8, s_height), GTextOverflowModeWordWrap, GTextAlignmentCenter);
			int height = calcSize.h + 3;	//Extra 3 for y
			if(height > 48)
				height = 48;
			//APP_LOG(APP_LOG_LEVEL_DEBUG, "%d,%d,%d", calcSize.w, calcSize.h, s_height - height - 10);
			layer_set_frame(text_layer_get_layer(s_weather_layer), GRect(4, s_height - height - 7, s_width - 8, height));
			strcpy(s_weather_layer_buffer, weather_layer_buffer);
			text_layer_set_text(s_weather_layer, s_weather_layer_buffer);
		}
	}
	
	if((temp_tuple = dict_find(iterator, MESSAGE_KEY_ICON))) {
		int icon = (int)temp_tuple->value->int32;
		if(icon != s_weather) {
			s_weather = icon;
			int resourceID = RESOURCE_ID_IMG_UNKNOWN;
			switch(icon) {
				case WEATHER_CLEAR_DAY:
					resourceID = RESOURCE_ID_IMG_CLEAR_DAY;
					break;
				case WEATHER_CLEAR_NIGHT:
					resourceID = RESOURCE_ID_IMG_CLEAR_NIGHT;
					break;
				case WEATHER_PARTLY_CLOUDY_DAY:
					resourceID = RESOURCE_ID_IMG_PARTLY_CLOUDY_DAY;
					break;
				case WEATHER_PARTLY_CLOUDY_NIGHT:
					resourceID = RESOURCE_ID_IMG_PARTLY_CLOUDY_NIGHT;
					break;
				case WEATHER_CLOUDY:
					resourceID = RESOURCE_ID_IMG_CLOUDY;
					break;
				case WEATHER_RAIN:
					resourceID = RESOURCE_ID_IMG_RAIN;
					break;
				case WEATHER_SNOW:
					resourceID = RESOURCE_ID_IMG_SNOW;
					break;
				case WEATHER_SLEET:
					resourceID = RESOURCE_ID_IMG_SLEET;
					break;
				case WEATHER_WIND:
					resourceID = RESOURCE_ID_IMG_WIND;
					break;
				case WEATHER_FOG:
					resourceID = RESOURCE_ID_IMG_FOG;
					break;
			}
			GBitmap *next = gbitmap_create_with_resource(resourceID);
			bitmap_layer_set_bitmap(s_weather_img_layer, next);
			gbitmap_destroy(s_weather_img);
			s_weather_img = next;
		}
	}
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}
static bool s_is_low_battery = false;
static void battery_update_proc(Layer *layer, GContext *ctx) {
	GRect bounds = layer_get_bounds(layer);
	
	// Find the width of the bar
	int width = (int)(float)(((float)s_battery_level / 100.0F) * (float)bounds.size.w);

	// Draw the background
	graphics_context_set_fill_color(ctx, GColorBlack);
	graphics_fill_rect(ctx, bounds, 0, GCornerNone);

	// Draw the bar
	graphics_context_set_fill_color(ctx, s_battery_level <= 25 ? GColorRed : s_battery_color);
	graphics_fill_rect(ctx, GRect(0, 0, width, BATTERY_HEIGHT), 0, GCornerNone);
	
	if(s_battery_level > 10 || s_is_charging > 0)
		s_is_low_battery = false;
	else if(!s_is_low_battery) {
		s_is_low_battery = true;
		char weather_layer_buffer[61] = "Low battery";

		GSize calcSize = graphics_text_layout_get_content_size(weather_layer_buffer, s_font_tiny,
			GRect(0, 0, s_width - 8, s_height), GTextOverflowModeWordWrap, GTextAlignmentCenter);
		int height = calcSize.h + 3;	//Extra 3 for y
		if(height > 48)
			height = 48;
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "%d,%d,%d", calcSize.w, calcSize.h, s_height - height - 10);
		layer_set_frame(text_layer_get_layer(s_weather_layer), GRect(4, s_height - height - 7, s_width - 8, height));
		strcpy(s_weather_layer_buffer, weather_layer_buffer);
		text_layer_set_text(s_weather_layer, s_weather_layer_buffer);
	}
}

static void battery_callback(BatteryChargeState state) {
	// Get the current state of the battery
	s_battery_level = state.charge_percent;
	int battery_state = state.is_charging ? 2 : (state.is_plugged ? 1 : 0);
	if(battery_state != s_is_charging) {
		s_is_charging = battery_state;
		if(battery_state == 0)
			bitmap_layer_set_bitmap(s_battery_img_layer, NULL);
		else if(battery_state == 1)
			bitmap_layer_set_bitmap(s_battery_img_layer, s_battery_plugged_in);
		else
			bitmap_layer_set_bitmap(s_battery_img_layer, s_battery_charging);
	}
	// Update meter
	layer_mark_dirty(s_battery_layer);
}

static void main_window_load(Window *window) {
  // Get information about the Window
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
	
	s_width = bounds.size.w;
	s_height = bounds.size.h;
	int top = PBL_IF_ROUND_ELSE(s_top - 6, s_top);	//34

  // Create the TextLayer with specific bounds
  s_time_layer = text_layer_create(
		GRect(0, top + 12, s_width - 30, 55));
  s_ampm_layer = text_layer_create(
		GRect(s_width - 28, top + 38, 28, 24));
  s_day_layer = text_layer_create(
		GRect(4, top + 63, s_width - 68, 24));
  s_date_layer = text_layer_create(
		GRect(s_width - 60, top + 63, 60, 24));
	s_temp_layer = text_layer_create(
		GRect(4, 4, s_width - 36, 30));
	s_next_temp_layer = text_layer_create(
		GRect(4, 32, s_width - 4, 30));
	
	s_weather_layer = text_layer_create(
		GRect(4, bounds.size.h - 40, s_width - 8, 36));
  
  // Create GFont
  s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TIME_42));
  s_font_small = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TIME_18));
	s_font_tiny = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TIME_15));

  // Improve the layout to be more like a watchface
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_font(s_time_layer, s_time_font);
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentRight);
  
  text_layer_set_background_color(s_ampm_layer, GColorClear);
  text_layer_set_text_color(s_ampm_layer, GColorChromeYellow);
  text_layer_set_font(s_ampm_layer, s_font_small);
  text_layer_set_text_alignment(s_ampm_layer, GTextAlignmentLeft);
  
  text_layer_set_background_color(s_day_layer, GColorClear);
  text_layer_set_text_color(s_day_layer, GColorChromeYellow);
  text_layer_set_font(s_day_layer, s_font_tiny);
  text_layer_set_text_alignment(s_day_layer, GTextAlignmentRight);
  
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorWhite);
  text_layer_set_font(s_date_layer, s_font_tiny);
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentLeft);
  
  text_layer_set_background_color(s_temp_layer, GColorClear);
  text_layer_set_text_color(s_temp_layer, GColorCyan);
  text_layer_set_font(s_temp_layer, s_font_small);
  text_layer_set_text_alignment(s_temp_layer, GTextAlignmentRight);
  
  text_layer_set_background_color(s_next_temp_layer, GColorClear);
  text_layer_set_text_color(s_next_temp_layer, GColorLightGray);
  text_layer_set_font(s_next_temp_layer, s_font_small);
  text_layer_set_text_alignment(s_next_temp_layer, GTextAlignmentRight);
  
  text_layer_set_background_color(s_weather_layer, GColorClear);
  text_layer_set_text_color(s_weather_layer, GColorChromeYellow);
  text_layer_set_font(s_weather_layer, s_font_tiny);
  text_layer_set_text_alignment(s_weather_layer, GTextAlignmentCenter);
	text_layer_set_text(s_weather_layer, "Loading...");
	text_layer_set_overflow_mode(s_weather_layer, GTextOverflowModeTrailingEllipsis);
	
	s_weather_img = gbitmap_create_with_resource(RESOURCE_ID_IMG_UNKNOWN);
	s_weather_img_layer = bitmap_layer_create(GRect(s_width - 32, 0, 32, 32));
	bitmap_layer_set_bitmap(s_weather_img_layer, s_weather_img);
	
	// Creating charging status
	s_battery_charging = gbitmap_create_with_resource(RESOURCE_ID_IMG_CHARGING);
	s_battery_plugged_in = gbitmap_create_with_resource(RESOURCE_ID_IMG_PLUGGED_IN);
	s_battery_img_layer = bitmap_layer_create(GRect(0, 0, 32, 32));
	
	// Create battery meter layer
	s_battery_layer = layer_create(GRect(0, bounds.size.h - BATTERY_HEIGHT, s_width, BATTERY_HEIGHT));
	layer_set_update_proc(s_battery_layer, battery_update_proc);

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_ampm_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_day_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_temp_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_next_temp_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_weather_layer));
	layer_add_child(window_layer, bitmap_layer_get_layer(s_weather_img_layer));
	layer_add_child(window_layer, bitmap_layer_get_layer(s_battery_img_layer));
  layer_add_child(window_layer, s_battery_layer);
	
	bt_handler(connection_service_peek_pebble_app_connection());
}

static void main_window_unload(Window *window) {
	// Destroy battery layer
	bitmap_layer_destroy(s_battery_img_layer);
	gbitmap_destroy(s_battery_charging);
	gbitmap_destroy(s_battery_plugged_in);
	layer_destroy(s_battery_layer);
	
	gbitmap_destroy(s_weather_img);
	bitmap_layer_destroy(s_weather_img_layer);
  // Destroy TextLayer
	text_layer_destroy(s_weather_layer);
  text_layer_destroy(s_next_temp_layer);
  text_layer_destroy(s_temp_layer);
  text_layer_destroy(s_day_layer);
  text_layer_destroy(s_date_layer);
  text_layer_destroy(s_ampm_layer);
  text_layer_destroy(s_time_layer);
  // Destroy Font
  fonts_unload_custom_font(s_font_tiny);
  fonts_unload_custom_font(s_font_small);
  fonts_unload_custom_font(s_time_font);
}

static void init() {
	s_battery_color = GColorVividCerulean;
	
  // Create main Window element and assign to pointer
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorBlack);

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);
  update_time();
	update_date();
	
	// Register with TickTimerService
	tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
	
	// Register with Bluetooth Service
	connection_service_subscribe((ConnectionHandlers) {
		.pebble_app_connection_handler = bt_handler
	});
	
	// Register callbacks
	app_message_register_inbox_received(inbox_received_callback);
	app_message_register_inbox_dropped(inbox_dropped_callback);
	app_message_register_outbox_failed(outbox_failed_callback);
	app_message_register_outbox_sent(outbox_sent_callback);
	
	// Open AppMessage
	app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
	
	// Register for battery level updates
	battery_state_service_subscribe(battery_callback);
	
	battery_callback(battery_state_service_peek());
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