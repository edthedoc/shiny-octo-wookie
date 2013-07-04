#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"
#include "stdlib.h"
#include "string.h"
#include "config.h"
#include "my_math.h"
#include "suncalc.h"
#include "time_layer.h"
#include "http.h"
#include "util.h"
#include "link_monitor.h"

// This is the Default APP_ID to work with old versions of httpebble
//#define MY_UUID { 0x91, 0x41, 0xB6, 0x28, 0xBC, 0x89, 0x49, 0x8E, 0xB1, 0x47, 0x04, 0x9F, 0x49, 0xC0, 0x99, 0xAD }

#define MY_UUID { 0x91, 0x41, 0xB6, 0x28, 0xBC, 0x89, 0x49, 0x8E, 0xB1, 0x47, 0x04, 0x9F, 0x49, 0xC0, 0x99, 0xAD }

PBL_APP_INFO(MY_UUID,
	     "Helvetime Weather", "Ed Hollingsbee",
	     1, 0, /* App major/minor version */
	     RESOURCE_ID_IMAGE_MENU_ICON,
	     APP_INFO_WATCH_FACE);

// from roboto weather erh
#define TIME_FRAME      (GRect(0, 45, 144, 168-6)

Window window;
TextLayer cwLayer; 					// The calendar week
TextLayer text_sunrise_layer;
TextLayer text_sunset_layer;
TextLayer text_temperature_layer;
TextLayer DayOfWeekLayer;
TextLayer MonthLayer;
TextLayer YearLayer;
TextLayer DateLayer;
TimeLayer time_layer;
BmpContainer background_image;
BmpContainer time_format_image;

static int our_latitude, our_longitude, our_timezone = 99;
static bool located = false;
static bool calculated_sunset_sunrise = false;
static bool temperature_set = false;

GFont font_temperature;        		/* font for Temperature */
GFont font_hour;					/* font for hour */
GFont font_minute;					/* font for minute */


#define TOTAL_WEATHER_IMAGES 1
BmpContainer weather_images[TOTAL_WEATHER_IMAGES];

const int WEATHER_IMAGE_RESOURCE_IDS[] = {
    RESOURCE_ID_IMAGE_CLEAR_DAY,
	RESOURCE_ID_IMAGE_CLEAR_NIGHT,
	RESOURCE_ID_IMAGE_RAIN,
	RESOURCE_ID_IMAGE_SNOW,
	RESOURCE_ID_IMAGE_SLEET,
	RESOURCE_ID_IMAGE_WIND,
	RESOURCE_ID_IMAGE_FOG,
	RESOURCE_ID_IMAGE_CLOUDY,
	RESOURCE_ID_IMAGE_PARTLY_CLOUDY_DAY,
	RESOURCE_ID_IMAGE_PARTLY_CLOUDY_NIGHT,
	RESOURCE_ID_IMAGE_NO_WEATHER
};

//pasted code
const unsigned int VIBE_INTERVAL_IN_MINUTES = 30;

const VibePattern HOUR_VIBE_PATTERN = {
  .durations = (uint32_t []) {50, 200, 50, 200, 50, 200, 50, 200},
  .num_segments = 8
};
const VibePattern PART_HOUR_INTERVAL_VIBE_PATTERN = {
  .durations = (uint32_t []) {50, 200, 50, 200},
  .num_segments = 4
};
///pasted code


void set_container_image(BmpContainer *bmp_container, const int resource_id, GPoint origin) {
  layer_remove_from_parent(&bmp_container->layer.layer);
  bmp_deinit_container(bmp_container);

  bmp_init_container(resource_id, bmp_container);

  GRect frame = layer_get_frame(&bmp_container->layer.layer);
  frame.origin.x = origin.x;
  frame.origin.y = origin.y;
  layer_set_frame(&bmp_container->layer.layer, frame);

  layer_add_child(&window.layer, &bmp_container->layer.layer);
}

unsigned short get_display_hour(unsigned short hour) {
  if (clock_is_24h_style()) {
    return hour;
  }

  unsigned short display_hour = hour % 12;

  // Converts "0" to "12"
  return display_hour ? display_hour : 12;
}


void adjustTimezone(float* time) 
{
  *time += our_timezone;
  if (*time > 24) *time -= 24;
  if (*time < 0) *time += 24;
}

// Calculating Sunrise/sunset with courtesy of Michael Ehrmann
// https://github.com/mehrmann/pebble-sunclock
void updateSunsetSunrise()
{
	static char sunrise_text[] = "00:00";
	static char sunset_text[]  = "00:00";
	
	PblTm pblTime;
	get_time(&pblTime);

	char *time_format;

	if (clock_is_24h_style()) 
	{
	  time_format = "%R";
	} 
	else 
	{
	  time_format = "%I:%M";
	}
	float sunriseTime = calcSunRise(pblTime.tm_year, pblTime.tm_mon+1, pblTime.tm_mday, our_latitude / 10000, our_longitude / 10000, 91.0f);
	float sunsetTime = calcSunSet(pblTime.tm_year, pblTime.tm_mon+1, pblTime.tm_mday, our_latitude / 10000, our_longitude / 10000, 91.0f);
	adjustTimezone(&sunriseTime);
	adjustTimezone(&sunsetTime);	
	if (!pblTime.tm_isdst) 
	{
	  sunriseTime+=1;
	  sunsetTime+=1;
	} 
	pblTime.tm_min = (int)(60*(sunriseTime-((int)(sunriseTime))));
	pblTime.tm_hour = (int)sunriseTime;
	string_format_time(sunrise_text, sizeof(sunrise_text), time_format, &pblTime);
	text_layer_set_text(&text_sunrise_layer, sunrise_text);

	pblTime.tm_min = (int)(60*(sunsetTime-((int)(sunsetTime))));
	pblTime.tm_hour = (int)sunsetTime;
	string_format_time(sunset_text, sizeof(sunset_text), time_format, &pblTime);
	text_layer_set_text(&text_sunset_layer, sunset_text);
}

unsigned short the_last_hour = 25;

void display_counters(TextLayer *dataLayer, struct Data d, int infoType) {
	
	static char temp_text[5];
	
	if(d.link_status != LinkStatusOK){
		memcpy(temp_text, "?", 1);
	}
	else {	
		if (infoType == 1) {
			if(d.missed) {
				memcpy(temp_text, itoa(d.missed), 4);
			}
			else {
				memcpy(temp_text, itoa(0), 4);
			}
		}
		else if(infoType == 2) {
			if(d.unread) {
				memcpy(temp_text, itoa(d.unread), 4);
			}
			else {
				memcpy(temp_text, itoa(0), 4);
			}
		}
	}
	
	text_layer_set_text(dataLayer, temp_text);
}

//Weather				 
void request_weather();
void failed(int32_t cookie, int http_status, void* context) {	
	if((cookie == 0 || cookie == WEATHER_HTTP_COOKIE) && !temperature_set) {
		set_container_image(&weather_images[0], WEATHER_IMAGE_RESOURCE_IDS[10], GPoint(16, 2));
		text_layer_set_text(&text_temperature_layer, "--°");
	}
	
	// link_monitor_handle_failure(http_status);
	
	//Re-request the location and subsequently weather on next minute tick
	// located = false;
}

void success(int32_t cookie, int http_status, DictionaryIterator* received, void* context) {
	
	if(cookie != WEATHER_HTTP_COOKIE) return;
	
	Tuple* icon_tuple = dict_find(received, WEATHER_KEY_ICON);
	if(icon_tuple) {
		int icon = icon_tuple->value->int8;
		if(icon >= 0 && icon < 10) {
			set_container_image(&weather_images[0], WEATHER_IMAGE_RESOURCE_IDS[icon], GPoint(16, 2));  // ---------- Weather Image
		} else {
			set_container_image(&weather_images[0], WEATHER_IMAGE_RESOURCE_IDS[10], GPoint(16, 2));
		}
	}
	
	Tuple* temperature_tuple = dict_find(received, WEATHER_KEY_TEMPERATURE);
	if(temperature_tuple) {
		
		static char temp_text[5];
		memcpy(temp_text, itoa(temperature_tuple->value->int16), 4);
		int degree_pos = strlen(temp_text);
		memcpy(&temp_text[degree_pos], "°", 3);
		text_layer_set_text(&text_temperature_layer, temp_text);
		temperature_set = true;
	}
	
	link_monitor_handle_success(&data);
	//display_counters(&calls_layer, data, 1);
	//display_counters(&sms_layer, data, 2);
}

void location(float latitude, float longitude, float altitude, float accuracy, void* context) {
	// Fix the floats
	our_latitude = latitude * 10000;
	our_longitude = longitude * 10000;
	located = true;
	request_weather();
}

void reconnect(void* context) {
	located = false;
	request_weather();
}



static void app_send_failed(DictionaryIterator* failed, AppMessageResult reason, void* context) {
	link_monitor_handle_failure(reason, &data);
	//display_counters(&calls_layer, data, 1);
	//display_counters(&sms_layer, data, 2);
}

void receivedtime(int32_t utc_offset_seconds, bool is_dst, uint32_t unixtime, const char* tz_name, void* context)
{	
	our_timezone = (utc_offset_seconds / 3600);
	if (is_dst)
	{
		our_timezone--;
	}
	
	if (located && our_timezone != 99 && !calculated_sunset_sunrise)
    {
        updateSunsetSunrise();
	    calculated_sunset_sunrise = true;
    }
}

void update_display(PblTm *current_time) {
	
  unsigned short display_hour = get_display_hour(current_time->tm_hour);
     
  if (the_last_hour != display_hour){
	  
	  // Day of week
	  text_layer_set_text(&DayOfWeekLayer, DAY_NAME_LANGUAGE[current_time->tm_wday]); 
	  
	  // Month adapted from day of week erh
	  text_layer_set_text(&MonthLayer, MONTH_NAME_LANGUAGE[current_time->tm_mon]); 

	  
	  
	 
	  if (!clock_is_24h_style()) {
		if (current_time->tm_hour >= 12) {
		  set_container_image(&time_format_image, RESOURCE_ID_IMAGE_PM_MODE, GPoint(118, 109));
		} else {
		  layer_remove_from_parent(&time_format_image.layer.layer);
		  bmp_deinit_container(&time_format_image);
		}

/* removing leading zero? duplicate
		if (display_hour/10 == 0) {
		  layer_remove_from_parent(&time_digits_images[0].layer.layer);
		  bmp_deinit_container(&time_digits_images[0]);
		}
*/
	  }
	  
	  // -------------------- Calendar week  
	  static char cw_text[] = "XXXXX00";
	  string_format_time(cw_text, sizeof(cw_text), TRANSLATION_CW , current_time);
	  text_layer_set_text(&cwLayer, cw_text); 
	  // ------------------- Calendar week  
	  
	  the_last_hour = display_hour;
  }
	
}


void handle_minute_tick(AppContextRef ctx, PebbleTickEvent *t) {
  (void)t;  //pasted code line
  (void)ctx;	
	
	
	
	//from roboto weather erh
    static char hour_text[] = "00";
    static char minute_text[] = " 00";
    if (clock_is_24h_style())
    {
        string_format_time(hour_text, sizeof(hour_text), "%H", t->tick_time);
    }
    else
    {
        string_format_time(hour_text, sizeof(hour_text), "%I", t->tick_time);
        if (hour_text[0] == '0')
        {
            /* This is a hack to get rid of the leading zero.*/
            memmove(&hour_text[0], &hour_text[1], sizeof(hour_text) - 1);
        }
    }

    string_format_time(minute_text, sizeof(minute_text), " %M", t->tick_time);
    time_layer_set_text(&time_layer, hour_text, minute_text);
		
// date and year from robot weather erh	
	static char date_text[] = "00"; //erh
//    if (t->units_changed & DAY_UNIT)
	    {
        string_format_time(date_text, sizeof(date_text), "%d", t->tick_time);
        text_layer_set_text(&DateLayer, date_text);
    }

	static char year_text[] = "0000"; //erh
//    if (t->units_changed & DAY_UNIT)
	    {
        string_format_time(year_text, sizeof(year_text), "20" "%y", t->tick_time);
        text_layer_set_text(&YearLayer, year_text);
    }

	
    update_display(t->tick_time);
	
//pasted code erh
//hourly and part hourly vibes
  static char timeText[] = "00:00"; // Needs to be static because it's used by the system later.

  PblTm currentTime;

  get_time(&currentTime);

  if ((currentTime.tm_min == 0) && ( 8 <= currentTime.tm_hour || currentTime.tm_hour <= 22)){ //on the hour and only from 8am to 10pm inclusive erh
    vibes_enqueue_custom_pattern(HOUR_VIBE_PATTERN);
  } else if (((currentTime.tm_min % VIBE_INTERVAL_IN_MINUTES) == 0) && ( 8 <= currentTime.tm_hour || currentTime.tm_hour <= 22)){
    vibes_enqueue_custom_pattern(PART_HOUR_INTERVAL_VIBE_PATTERN);
  }
//pasted code
	if(!located || !(t->tick_time->tm_min % 15))
	{
		// Every 15 minutes, request updated weather
		http_location_request();
	}
		// Every 15 minutes, request updated time - not sure why this isn't within the previous if - erh
		http_time_request();
		
	if(!calculated_sunset_sunrise)
    {
	    // Start with some default values - changed from "Wait!"
	    text_layer_set_text(&text_sunrise_layer, "-----");
	    text_layer_set_text(&text_sunset_layer, "-----");
    }
	
	if(!(t->tick_time->tm_min % 2) || data.link_status == LinkStatusUnknown) link_monitor_ping();
}

void handle_init(AppContextRef ctx) {
  (void)ctx;

	ResHandle res_t;
	ResHandle res_h;
	ResHandle res_m;
	
	window_init(&window, "Helvetime Weather");
	window_stack_push(&window, true /* Animated */);  
	window_set_background_color(&window, GColorBlack);
	resource_init_current_app(&APP_RESOURCES);

	// Load Fonts
	res_t = resource_get_handle(RESOURCE_ID_FONT_ROBOTO_LIGHT_SUBSET_42);
    res_h = resource_get_handle(RESOURCE_ID_FONT_HELVETICA_NEUE_BOLD_SUBSET_60);
    res_m = resource_get_handle(RESOURCE_ID_FONT_HELVETICA_NEUE_BOLD_SUBSET_60);
	
    font_temperature = fonts_load_custom_font(res_t);
	font_hour = fonts_load_custom_font(res_h);
	font_minute = fonts_load_custom_font(res_m);

	bmp_init_container(RESOURCE_ID_IMAGE_BACKGROUND, &background_image);
	layer_add_child(&window.layer, &background_image.layer.layer);
	
if (clock_is_24h_style()) {
		bmp_init_container(RESOURCE_ID_IMAGE_24_HOUR_MODE, &time_format_image);

		time_format_image.layer.layer.frame.origin.x = 118;
		time_format_image.layer.layer.frame.origin.y = 111;
		layer_add_child(&window.layer, &time_format_image.layer.layer);
	}

	// Calendar Week Text
	text_layer_init(&cwLayer, GRect(51, 152, 80 /* width */, 30 /* height */));
	layer_add_child(&background_image.layer.layer, &cwLayer.layer);
	text_layer_set_text_color(&cwLayer, GColorWhite);
	text_layer_set_background_color(&cwLayer, GColorClear);
	text_layer_set_font(&cwLayer, fonts_get_system_font(FONT_KEY_GOTHIC_14));

	// Sunrise Text
	text_layer_init(&text_sunrise_layer, window.layer.frame);
	text_layer_set_text_color(&text_sunrise_layer, GColorWhite);
	text_layer_set_background_color(&text_sunrise_layer, GColorClear);
	layer_set_frame(&text_sunrise_layer.layer, GRect(9, 152, 100, 30));
	text_layer_set_font(&text_sunrise_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
	layer_add_child(&window.layer, &text_sunrise_layer.layer);

	// Sunset Text
	text_layer_init(&text_sunset_layer, window.layer.frame);
	text_layer_set_text_color(&text_sunset_layer, GColorWhite);
	text_layer_set_background_color(&text_sunset_layer, GColorClear);
	layer_set_frame(&text_sunset_layer.layer, GRect(114, 152, 100, 30));
	text_layer_set_font(&text_sunset_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
	layer_add_child(&window.layer, &text_sunset_layer.layer); 
  
	// Text for Temperature
	text_layer_init(&text_temperature_layer, window.layer.frame);
	text_layer_set_text_color(&text_temperature_layer, GColorWhite);
	text_layer_set_background_color(&text_temperature_layer, GColorClear);
	layer_set_frame(&text_temperature_layer.layer, GRect(68, 1, 64, 68));
	text_layer_set_font(&text_temperature_layer, font_temperature);
	layer_add_child(&window.layer, &text_temperature_layer.layer);  
  
	// Day of week text
	text_layer_init(&DayOfWeekLayer, GRect(4, 115, 130 /* width */, 30 /* height */));
	layer_add_child(&background_image.layer.layer, &DayOfWeekLayer.layer);
	text_layer_set_text_color(&DayOfWeekLayer, GColorWhite);
	text_layer_set_background_color(&DayOfWeekLayer, GColorClear);
	text_layer_set_font(&DayOfWeekLayer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));

	// Month text adapted from day erh
	text_layer_init(&MonthLayer, GRect(67, 115, 130 /* width */, 30 /* height */));
	layer_add_child(&background_image.layer.layer, &MonthLayer.layer);
	text_layer_set_text_color(&MonthLayer, GColorWhite);
	text_layer_set_background_color(&MonthLayer, GColorClear);
	text_layer_set_font(&MonthLayer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));

	
// added by ed for text date and year
	// Date text adapted from day
	text_layer_init(&DateLayer, GRect(42, 115, 130 /* width */, 30 /* height */));
	layer_add_child(&background_image.layer.layer, &DateLayer.layer);
	text_layer_set_text_color(&DateLayer, GColorWhite);
	text_layer_set_background_color(&DateLayer, GColorClear);
	text_layer_set_font(&DateLayer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));

	// Year text adapted from day
	text_layer_init(&YearLayer, GRect(99, 115, 130 /* width */, 30 /* height */));
	layer_add_child(&background_image.layer.layer, &YearLayer.layer);
	text_layer_set_text_color(&YearLayer, GColorWhite);
	text_layer_set_background_color(&YearLayer, GColorClear);
	text_layer_set_font(&YearLayer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));

 //added by erh for time display	from roboto weather
    time_layer_init(&time_layer, window.layer.frame);
    time_layer_set_text_color(&time_layer, GColorWhite);
    time_layer_set_background_color(&time_layer, GColorClear);
    time_layer_set_fonts(&time_layer, font_hour, font_minute);
    layer_set_frame(&time_layer.layer, TIME_FRAME));
    layer_add_child(&window.layer, &time_layer.layer);

	
	data.link_status = LinkStatusUnknown;
	
	//every minute ping the phone
	link_monitor_ping();
	
	// request data refresh on window appear (for example after notification)
	WindowHandlers handlers = { .appear = &link_monitor_ping};
	window_set_window_handlers(&window, handlers);
	
    http_register_callbacks((HTTPCallbacks){.failure=failed,.success=success,.reconnect=reconnect,.location=location,.time=receivedtime}, (void*)ctx);
    	
    // Avoids a blank screen on watch start.
    PblTm tick_time;

    get_time(&tick_time);
    update_display(&tick_time);

}


void handle_deinit(AppContextRef ctx) {

	bmp_deinit_container(&background_image);
	
	fonts_unload_custom_font(font_temperature);
	fonts_unload_custom_font(font_hour);
	fonts_unload_custom_font(font_minute);

}

void pbl_main(void *params)
{
  PebbleAppHandlers handlers =
  {
    .init_handler = &handle_init,
    .deinit_handler = &handle_deinit,
    .tick_info =
	{
      .tick_handler = &handle_minute_tick,
      .tick_units = MINUTE_UNIT
    },
	.messaging_info = {
		.buffer_sizes = {
			.inbound = 124,
			.outbound = 256,
		}
	}
  };
	
  app_event_loop(params, &handlers);
}

void request_weather() {
	if(!located) {
		http_location_request();
		return;
	}
	
	// Build the HTTP request
	DictionaryIterator *body;
	HTTPResult result = http_out_get("https://ofkorth.net/pebble/weather.php", WEATHER_HTTP_COOKIE, &body);
	if(result != HTTP_OK) {
		return;
	}
	
	dict_write_int32(body, WEATHER_KEY_LATITUDE, our_latitude);
	dict_write_int32(body, WEATHER_KEY_LONGITUDE, our_longitude);
	dict_write_cstring(body, WEATHER_KEY_UNIT_SYSTEM, UNIT_SYSTEM);
	
	// Send it.
	if(http_out_send() != HTTP_OK) {
		return;
	}	
}
