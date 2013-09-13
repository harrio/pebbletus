#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"

//CA3023FF-DC97-467B-A161-F7AE32FE29E0
#define UUID { 0xca, 0x30, 0x23, 0xff, 0xdc, 0x97, 0x46, 0x7b, 0xa1, 0x61, 0xf7, 0xae, 0x32, 0xfe, 0x29, 0xe0 }

PBL_APP_INFO(UUID, "Impetus", "Harri Ohra-aho", 1, 0, RESOURCE_ID_IMAGE_ICON_APP, APP_INFO_STANDARD_APP);

//Globals
Window window;
TextLayer phaseLayer;
TextLayer timeLayer;
TextLayer roundLayer;
TextLayer presetLayer;
ActionBarLayer actionBarLayer;

HeapBitmap button_image_play;
HeapBitmap button_image_pause;
HeapBitmap button_image_reset;

//Set of enumerated keys with names and values. Identical to Android app keys
enum {
  DATA_KEY = 0x0,
  SELECT_KEY = 0x0, // TUPLE_INTEGER
  UP_KEY = 0x01,
  DOWN_KEY = 0x02,
  INIT_KEY = 0x03,
  PHASE_KEY = 0x01,
  ROUND_KEY = 0x02,
  CHANGE_KEY = 0x03,
  TIME_KEY = 0x04,
  PRESET_KEY = 0x05,
  CHANGE_STARTED = 0x0,
  CHANGE_PAUSED = 0x01,
  CHANGE_STOPPED = 0x02,
  STATUS = 0x03,
  WORK = 0x04,
  REST = 0x05,
  FINISHED = 0x06,
  INIT = 0x07
};

enum State {
  STOPPED,
  PAUSED,
  RUNNING
};

enum State current_state = STOPPED;
int total_seconds = 0;
int current_seconds = 0;
int last_set_time = -1;

static const uint32_t const workSegments[] = { 100, 100, 100, 100, 100, 100, 100, 100, 100 };
VibePattern workPat = {
  .durations = workSegments,
  .num_segments = ARRAY_LENGTH(workSegments)
};

static const uint32_t const restSegments[] = { 400, 200, 400 };
VibePattern restPat = {
  .durations = restSegments,
  .num_segments = ARRAY_LENGTH(restSegments)
};

static const uint32_t const finSegments[] = { 600, 300, 600, 300, 600 };
VibePattern finPat = {
  .durations = finSegments,
  .num_segments = ARRAY_LENGTH(finSegments)
};

void update_time() {
  if (current_seconds == last_set_time) {
    return;
  }

  static char time_text[] = "00:00";
  if (current_seconds >= 0) {
    PblTm time;
    time.tm_min  = current_seconds / 60;
    time.tm_sec  = current_seconds - time.tm_min * 60;

    string_format_time(time_text, sizeof(time_text), "%M:%S", &time);
  }
  text_layer_set_text(&timeLayer, time_text);
  last_set_time = current_seconds;
}

void set_running() {
  current_state = RUNNING;
  action_bar_layer_set_icon(&actionBarLayer, BUTTON_ID_UP, &button_image_pause.bmp);
} 

void set_paused() {
  current_state = PAUSED;
  action_bar_layer_set_icon(&actionBarLayer, BUTTON_ID_UP, &button_image_play.bmp);
} 

void set_stopped() {
  current_state = STOPPED;
  action_bar_layer_set_icon(&actionBarLayer, BUTTON_ID_UP, &button_image_play.bmp);
  app_comm_set_sniff_interval(SNIFF_INTERVAL_NORMAL);
} 

void change_received(int change) {
  if (change == WORK) { 
      vibes_enqueue_custom_pattern(workPat);
      light_enable_interaction();
      set_running();
  } else if (change == REST) {
    vibes_enqueue_custom_pattern(restPat);
    light_enable_interaction();
    set_running();
  } else if (change == FINISHED) {
    vibes_enqueue_custom_pattern(finPat);
    light_enable_interaction();
    set_stopped();
  } else if (change == STATUS) {
    update_time();
  } else if (change == CHANGE_STOPPED) {
    vibes_double_pulse();
    set_stopped();
  } else if (change == CHANGE_STARTED) {
    vibes_double_pulse();    
    set_running();
  } else if (change == CHANGE_PAUSED) {
    vibes_double_pulse();    
    set_paused();
  }
}

void handle_second_counting_down() {
  current_seconds--;

  update_time();

  if (current_seconds == 0) {
    current_state = STOPPED;
  }
}

void handle_second_waiting() {
  current_seconds = total_seconds;
  update_time();
}

void handle_second_tick(AppContextRef ctx, PebbleTickEvent *t) {
  switch(current_state) {
  case PAUSED:
  case STOPPED:
    handle_second_waiting();
    break;
  case RUNNING:
    handle_second_counting_down();
    break;
  default:
    break;
  }
}

/**
  * Handler for AppMessage sent
  */
void out_sent_handler(DictionaryIterator *sent, void *context) {
}

/**
  * Handler for AppMessage send failed
  */
static void out_fail_handler(DictionaryIterator* failed, AppMessageResult reason, void* context) {
  //Notify the watch app user that the send operation failed
  text_layer_set_text(&phaseLayer, "Offline");
}

/**
  * Handler for received AppMessage
  */
static void in_received_handler(DictionaryIterator* iter, void* context) {
  
  Tuple *change_tuple = dict_find(iter, CHANGE_KEY);
  if (change_tuple) {
    int change = change_tuple->value->uint32;
    change_received(change); 
  }

  Tuple *preset_tuple = dict_find(iter, PRESET_KEY);
  if (preset_tuple) {
    static char preset[11];
    strcpy(preset, preset_tuple->value->cstring);
    text_layer_set_text(&presetLayer, preset);
  }

  Tuple *round_tuple = dict_find(iter, ROUND_KEY);
  if (round_tuple) {
    static char round[11];
    strcpy(round, round_tuple->value->cstring);
    text_layer_set_text(&roundLayer, round);
  }

  Tuple *phase_tuple = dict_find(iter, PHASE_KEY);
  if (phase_tuple) {
    static char phase[11];
    strcpy(phase, phase_tuple->value->cstring);
    text_layer_set_text(&phaseLayer, phase);
  }
  
  Tuple *time_tuple = dict_find(iter, TIME_KEY);
  if (time_tuple) {
    total_seconds = time_tuple->value->uint32;
    current_seconds = total_seconds;
  }  
}

/**
  * Handler for received message dropped
  */
void in_drop_handler(void *context, AppMessageResult reason) {
  //Notify the watch app user that the recieved message was dropped
  //text_layer_set_text(&phaseLayer, "Connection failed");
}

/**
  * Function to send a key press using a pre-agreed key
  */
static void send_cmd(uint8_t cmd) { //uint8_t is an unsigned 8-bit int (0 - 255)
  //Create a key-value pair
  Tuplet value = TupletInteger(DATA_KEY, cmd);
  
  //Construct the dictionary
  DictionaryIterator *iter;
  app_message_out_get(&iter);
  
  //If not constructed, do not send - return straight away
  if (iter == NULL)
    return;
  
  //Write the tuplet to the dictionary
  dict_write_tuplet(iter, &value);
  dict_write_end(iter);
  
  //Send the dictionary and release the buffer
  app_message_out_send();
  app_message_out_release();
}

/**
  * Handler for up click
  */
void up_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
  (void)recognizer;
  (void)window;
  
  //Send the UP_KEY
  send_cmd(UP_KEY);
  app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);
}

/**
  * Handler for down click
  */
void down_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
  (void)recognizer;
  (void)window;
  
  //Send the DOWN_KEY
  send_cmd(DOWN_KEY);
  app_comm_set_sniff_interval(SNIFF_INTERVAL_NORMAL);
}

/**
  * Handler for select click
  */
void select_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
  (void)recognizer;
  (void)window;
  
  //Send the SELECT_KEY
  //send_cmd(SELECT_KEY);
}

/**
  * Click config function
  */
void click_config_provider(ClickConfig **config, Window *window) {
  (void)window;

  config[BUTTON_ID_SELECT]->click.handler = (ClickHandler) select_single_click_handler;
  config[BUTTON_ID_UP]->click.handler = (ClickHandler) up_single_click_handler;
  config[BUTTON_ID_UP]->click.repeat_interval_ms = 100;
  config[BUTTON_ID_DOWN]->click.handler = (ClickHandler) down_single_click_handler;
  config[BUTTON_ID_DOWN]->click.repeat_interval_ms = 100;
}

void handle_main_appear(Window *window)
{
    // We need to add the action_bar when the main-window appears. If we do this in handle_init it picks up wrong window-bounds and the size doesn't fit.
    action_bar_layer_add_to_window(&actionBarLayer, window);
}

void handle_main_disappear(Window *window)
{
    // Since we add the layer on each appear, we remove it on each disappear.
    action_bar_layer_remove_from_window(&actionBarLayer);
}

/**
  * Resource initialisation handle function
  */
void handle_init(AppContextRef ctx) {
  (void)ctx;

  //Init window
  window_init(&window, "Main window");
  window_set_window_handlers(&window, (WindowHandlers) {
        .appear = (WindowHandler)handle_main_appear,
        .disappear = (WindowHandler)handle_main_disappear
  });
  window_stack_push(&window, true);
  window_set_background_color(&window, GColorBlack);
  
  ///Init resources
  resource_init_current_app(&APP_RESOURCES);

  heap_bitmap_init(&button_image_play, RESOURCE_ID_IMAGE_BUTTON_PLAY);
  heap_bitmap_init(&button_image_pause, RESOURCE_ID_IMAGE_BUTTON_PAUSE);
  heap_bitmap_init(&button_image_reset, RESOURCE_ID_IMAGE_BUTTON_RESET);  

  action_bar_layer_init(&actionBarLayer);
  action_bar_layer_set_click_config_provider(&actionBarLayer, (ClickConfigProvider) click_config_provider);
  action_bar_layer_set_icon(&actionBarLayer, BUTTON_ID_UP, &button_image_play.bmp);
  //action_bar_layer_set_icon(&actionBarLayer, BUTTON_ID_SELECT, &button_image_setup.bmp);
  action_bar_layer_set_icon(&actionBarLayer, BUTTON_ID_DOWN, &button_image_reset.bmp);

  //Init TextLayers
  text_layer_init(&phaseLayer, GRect(0, 0, 144, 38));
  text_layer_set_background_color(&phaseLayer, GColorWhite);
  text_layer_set_text_color(&phaseLayer, GColorBlack);
  text_layer_set_text_alignment(&phaseLayer, GTextAlignmentCenter);
  text_layer_set_font(&phaseLayer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));

  text_layer_init(&roundLayer, GRect(0, 38, 144, 38));
  text_layer_set_background_color(&roundLayer, GColorWhite);
  text_layer_set_text_color(&roundLayer, GColorBlack);
  text_layer_set_text_alignment(&roundLayer, GTextAlignmentCenter);
  text_layer_set_font(&roundLayer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));

  text_layer_init(&timeLayer, GRect(0, 76, 144, 38));
  text_layer_set_background_color(&timeLayer, GColorBlack);
  text_layer_set_text_color(&timeLayer, GColorWhite);
  text_layer_set_text_alignment(&timeLayer, GTextAlignmentCenter);
  text_layer_set_font(&timeLayer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));

  text_layer_init(&presetLayer, GRect(0, 114, 144, 38));
  text_layer_set_background_color(&presetLayer, GColorWhite);
  text_layer_set_text_color(&presetLayer, GColorBlack);
  text_layer_set_text_alignment(&presetLayer, GTextAlignmentCenter);
  text_layer_set_font(&presetLayer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));

  layer_add_child(&window.layer, &phaseLayer.layer);
  layer_add_child(&window.layer, &roundLayer.layer);
  layer_add_child(&window.layer, &timeLayer.layer);
  layer_add_child(&window.layer, &presetLayer.layer);
  text_layer_set_text(&phaseLayer, "Ready");
  
  //Setup button click handlers
  window_set_click_config_provider(&window, (ClickConfigProvider) click_config_provider);
  send_cmd(INIT_KEY);
}

void handle_deinit(AppContextRef ctx)
{
    heap_bitmap_deinit(&button_image_play);
    heap_bitmap_deinit(&button_image_pause);
    heap_bitmap_deinit(&button_image_reset);

    window_deinit(&window);
}

/**
  * Main Pebble loop
  */
void pbl_main(void *params) {
  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,
    
    .messaging_info = {
      //Set the sizes of the buffers
      .buffer_sizes = {
        .inbound = 64,
        .outbound = 16,
      },
     
      //Use the default callback node  
      .default_callbacks.callbacks = {
        .out_sent = out_sent_handler,
        .out_failed = out_fail_handler,
        .in_received = in_received_handler,
        .in_dropped = in_drop_handler,
      }
    },
    .tick_info = {
      .tick_handler = &handle_second_tick,
      .tick_units = SECOND_UNIT
    }
  };
  app_event_loop(params, &handlers);

}
