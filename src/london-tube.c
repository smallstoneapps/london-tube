/*
 * London Tube
 * Copyright (C) 2013 Matthew Tole
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"
#include "smallstone.h"
#include "config.h"
#include "http.h"

#if ROCKSHOT
#include "httpcapture.h"
#endif

// See https://gist.github.com/matthewtole/6144056 for explanation.
#if ANDROID
#define MY_UUID { 0x91, 0x41, 0xB6, 0x28, 0xBC, 0x89, 0x49, 0x8E, 0xB1, 0x47, 0xC8, 0x84, 0xF0, 0x16, 0x02, 0x15 }
#else
#define MY_UUID HTTP_UUID
#endif

PBL_APP_INFO(MY_UUID, "London Tube", "Matthew Tole", 1, 0,  RESOURCE_ID_MENU_ICON, APP_INFO_STANDARD_APP);

typedef struct {
  char code[3];
  int status;
  char name[20];
} TubeLine;

static TubeLine lines[] = {
  { "BL\0", 0, "Bakerloo" },
  { "CE\0", 0, "Central" },
  { "CI\0", 0, "Circle" },
  { "DI\0", 0, "District" },
  { "DL\0", 0, "DLR" },
  { "HC\0", 0, "H'smith & City" },
  { "JL\0", 0, "Jubilee" },
  { "ME\0", 0, "Metropolitan" },
  { "NO\0", 0, "Northern" },
  { "OV\0", 0, "Overground" },
  { "PI\0", 0, "Picadilly" },
  { "VI\0", 0, "Victoria" },
  { "WC\0", 0, "Waterloo & City" }
};

#define NUM_LINES 13

#define HTTP_COOKIE_STATUS 8823

Window window;
MenuLayer layer_menu;

HeapBitmap menu_icons[3];
#define MENU_ICON_OK 0
#define MENU_ICON_PROBLEM 1
#define MENU_ICON_UNKNOWN 2

#define STATE_UPDATING 0
#define STATE_OK 1
#define STATE_ERROR 2

#define SECTION_LINES 0
#define SECTION_OPTIONS 1

void handle_init(AppContextRef ctx);
void window_load(Window *me);
void window_unload(Window *me);
uint16_t menu_get_num_sections_callback(MenuLayer *me, void *data);
uint16_t menu_get_num_rows_callback(MenuLayer *me, uint16_t section_index, void *data);
int16_t menu_get_header_height_callback(MenuLayer *me, uint16_t section_index, void *data);
void menu_draw_header_callback(GContext* ctx, const Layer *cell_layer, uint16_t section_index, void *data);
void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data);
void menu_select_click_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context);
void do_status_request();
TubeLine* get_line_by_code(const char* code);

int xatoi (char** str, long* res);
int state = STATE_UPDATING;

char* line_order = "BLCECIDIDLHCJLMENOOVPIVIWC";

void http_reconnect(void* context);
void http_location(float latitude, float longitude, float altitude, float accuracy, void* context);
void http_failure(int32_t cookie, int http_status, void* context);
void http_success(int32_t cookie, int http_status, DictionaryIterator* received, void* context);

void pbl_main(void *params) {

  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,
    .messaging_info = {
      .buffer_sizes = {
        .inbound = 500,
        .outbound = 128,
      }
    }
  };

  #if ROCKSHOT
  http_capture_main(&handlers);
  #endif

  app_event_loop(params, &handlers);
}

void handle_init(AppContextRef ctx) {
  http_set_app_id(76782703);

  resource_init_current_app(&APP_RESOURCES);

  window_init(&window, "London Tube Window");
  window_stack_push(&window, true);

  create_thanks_window();

  http_register_callbacks((HTTPCallbacks){
    .failure=http_failure,
    .success=http_success,
    .reconnect=http_reconnect
  }, (void*)ctx);

  #if ROCKSHOT
  http_capture_init(ctx);
  #endif

  window_set_window_handlers(&window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
  });

}

void window_load(Window* me) {

  heap_bitmap_init(&menu_icons[MENU_ICON_OK], RESOURCE_ID_MENU_OK);
  heap_bitmap_init(&menu_icons[MENU_ICON_PROBLEM], RESOURCE_ID_MENU_PROBLEM);
  heap_bitmap_init(&menu_icons[MENU_ICON_UNKNOWN], RESOURCE_ID_MENU_UNKNOWN);

  menu_layer_init(&layer_menu, me->layer.bounds);

  menu_layer_set_callbacks(&layer_menu, NULL, (MenuLayerCallbacks){
    .get_num_sections = menu_get_num_sections_callback,
    .get_num_rows = menu_get_num_rows_callback,
    .get_header_height = menu_get_header_height_callback,
    .draw_header = menu_draw_header_callback,
    .draw_row = menu_draw_row_callback,
    .select_click = menu_select_click_callback
  });

  menu_layer_set_click_config_onto_window(&layer_menu, me);

  layer_add_child(&me->layer, menu_layer_get_layer(&layer_menu));

  do_status_request();
}

void do_status_request() {
  state = STATE_UPDATING;
  menu_layer_reload_data(&layer_menu);

  DictionaryIterator* body;
  HTTPResult result = http_out_get("http://api.pblweb.com/london-tube/v1/status.php", true, HTTP_COOKIE_STATUS, &body);
  if (result != HTTP_OK) {
    state = STATE_ERROR;
    menu_layer_reload_data(&layer_menu);
    return;
  }

  dict_write_cstring(body, 0, line_order);
  result = http_out_send();
  if (result != HTTP_OK) {
    state = STATE_ERROR;
    menu_layer_reload_data(&layer_menu);
  }
}

void window_unload(Window* me) {

  heap_bitmap_deinit(&menu_icons[0]);
  heap_bitmap_deinit(&menu_icons[1]);
  heap_bitmap_deinit(&menu_icons[2]);

}

uint16_t menu_get_num_sections_callback(MenuLayer *me, void *data) {
  return 2;
}

uint16_t menu_get_num_rows_callback(MenuLayer *me, uint16_t section_index, void *data) {
  switch (section_index) {
    case SECTION_LINES:
      return NUM_LINES;
    break;
    case SECTION_OPTIONS:
      return 2;
    break;
  }
  return 0;
}

int16_t menu_get_header_height_callback(MenuLayer *me, uint16_t section_index, void *data) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

void menu_draw_header_callback(GContext* ctx, const Layer *cell_layer, uint16_t section_index, void *data) {
  switch (section_index) {
    case SECTION_LINES: {
      switch (state) {
        case STATE_UPDATING:
          menu_cell_basic_header_draw(ctx, cell_layer, "Updating...");
        break;
        case STATE_OK: {
          char time_str[50];
          PblTm now;
          get_time(&now);
          if (clock_is_24h_style()) {
            string_format_time(time_str, sizeof(time_str), "Last Updated: %H:%M", &now);
          }
          else {
            string_format_time(time_str, sizeof(time_str), "Last Updated: %l:%M %p", &now);
          }
          menu_cell_basic_header_draw(ctx, cell_layer, time_str);
        }
        break;
        case STATE_ERROR:
          menu_cell_basic_header_draw(ctx, cell_layer, "Updating Failed");
        break;
      }
    }
    break;
    case SECTION_OPTIONS:
      menu_cell_basic_header_draw(ctx, cell_layer, "Options");
    break;
  }
}

void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
  switch (cell_index->section) {
    case 0: {
      char status_label[50];
      GBitmap* bmp;

      switch (lines[cell_index->row].status) {
        case 0:
          strcpy(status_label, "Getting Status");
          bmp = &menu_icons[MENU_ICON_UNKNOWN].bmp;
        break;
        case 1:
          strcpy(status_label, "Good Service");
          bmp = &menu_icons[MENU_ICON_OK].bmp;
        break;
        case 2:
          strcpy(status_label, "Part Closure");
          bmp = &menu_icons[MENU_ICON_PROBLEM].bmp;
        break;
        case 4:
          strcpy(status_label, "Minor Delays");
          bmp = &menu_icons[MENU_ICON_PROBLEM].bmp;
        break;
        case 8:
          strcpy(status_label, "Severe Delays");
          bmp = &menu_icons[MENU_ICON_PROBLEM].bmp;
        break;
        case 12:
          strcpy(status_label, "Severe & Minor Delays");
          bmp = &menu_icons[MENU_ICON_PROBLEM].bmp;
        break;
        default:
          strcpy(status_label, "Unknown Status");
          bmp = &menu_icons[MENU_ICON_UNKNOWN].bmp;
      }
      menu_cell_basic_draw(ctx, cell_layer, lines[cell_index->row].name, status_label, bmp);
    }
    break;
    case 1: {
      switch (cell_index->row) {
        case 0:
          menu_cell_basic_draw(ctx, cell_layer, "Refresh", "Updates the Tube status.", NULL);
        break;
        case 1:
          menu_cell_basic_draw(ctx, cell_layer, "Say Thanks", "Thanks the developer.", NULL);
        break;
      }
    }
  }
}

void menu_select_click_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context) {
  switch (cell_index->section) {
    case 1: {
      switch (cell_index->row) {
        case 0: {
          do_status_request();
          MenuIndex index =  { 0, 0 };
          menu_layer_set_selected_index(menu_layer, index, MenuRowAlignBottom, false);
        }
        break;
        case 1:
          send_thanks("london-tube", "1-0");
          show_thanks_window();
        break;
      }
    }
    break;
  }
}

void http_reconnect(void* context) {
}

void http_failure(int32_t cookie, int http_status, void* context) {
  if (cookie != HTTP_COOKIE_STATUS) {
    return;
  }

  state = STATE_ERROR;
  menu_layer_reload_data(&layer_menu);
}

void http_success(int32_t cookie, int http_status, DictionaryIterator* received, void* context) {

  switch (cookie) {
    case HTTP_COOKIE_STATUS: {
      Tuple* tuple_order = dict_find(received, 0);
      Tuple* tuple_statuses = dict_find(received, 1);

      const char* order = tuple_order->value->cstring;
      const char* statuses = tuple_statuses->value->cstring;

      for (int l = 0; l < 13; l += 1) {
        char code_str[2];
        code_str[0] = order[(l * 2)];
        code_str[1] = order[(l * 2) + 1];

        TubeLine* line = get_line_by_code(code_str);

        if (strcmp(line->code, code_str) != 0) {
          continue;
        }

        char* status_str = "00";
        status_str[0] = statuses[(l * 2)];
        status_str[1] = statuses[(l * 2) + 1];

        long foo;
        xatoi(&status_str, &foo);
        line->status = (int)foo;
      }
      state = STATE_OK;
      menu_layer_reload_data(&layer_menu);
    }
    break;
  }
}

TubeLine* get_line_by_code(const char* code) {
  for (int l = 0; l < NUM_LINES; l += 1) {
    if (strcmp(lines[l].code, code) == 0) {
      return &lines[l];
    }
  }
  return NULL;
}

/* This function is copied from the Embedded String Functions which
 * is available at http://elm-chan.org/fsw/strf/xprintf.html
 *
 * Since I'm only using it to convert to decimal numbers I should probably
 * rewrite it to make it simpler / more efficient.
 */

int xatoi (     /* 0:Failed, 1:Successful */
  char **str,   /* Pointer to pointer to the string */
  long *res   /* Pointer to the valiable to store the value */
)
{
  unsigned long val;
  unsigned char c, r, s = 0;


  *res = 0;

  while ((c = **str) == ' ') (*str)++;  /* Skip leading spaces */

  if (c == '-') {   /* negative? */
    s = 1;
    c = *(++(*str));
  }

  if (c == '0') {
    c = *(++(*str));
    switch (c) {
    case 'x':   /* hexdecimal */
      r = 16; c = *(++(*str));
      break;
    case 'b':   /* binary */
      r = 2; c = *(++(*str));
      break;
    default:
      if (c <= ' ') return 1; /* single zero */
      if (c < '0' || c > '9') return 0; /* invalid char */
      r = 8;    /* octal */
    }
  } else {
    if (c < '0' || c > '9') return 0; /* EOL or invalid char */
    r = 10;     /* decimal */
  }

  val = 0;
  while (c > ' ') {
    if (c >= 'a') c -= 0x20;
    c -= '0';
    if (c >= 17) {
      c -= 7;
      if (c <= 9) return 0; /* invalid char */
    }
    if (c >= r) return 0;   /* invalid char for current radix */
    val = val * r + c;
    c = *(++(*str));
  }
  if (s) val = 0 - val;     /* apply sign if needed */

  *res = val;
  return 1;
}
