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
#include "config.h"
#include "http.h"
#include "wnd-tube-status.h"

typedef struct {
  char code[3];
  int status;
  char name[20];
  int ordering;
} TubeLine;

#define max(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a > _b ? _a : _b; })

#define NUM_LINES 13
#define NUM_ICONS 3

#define MENU_ICON_OK 0
#define MENU_ICON_PROBLEM 1
#define MENU_ICON_UNKNOWN 2

#define STATE_UPDATING 0
#define STATE_OK 1
#define STATE_ERROR 2

#define SECTION_LINES 0
#define SECTION_OPTIONS 1

#define FONT_ROW_HEADER 0
#define FONT_ROW_BODY 1

static void window_load(Window *me);
static void window_unload(Window *me);
static void init_menu(Window* wnd);
static void load_bitmaps();
static void unload_bitmaps();
static uint16_t menu_get_num_sections_callback(MenuLayer *me, void *data);
static uint16_t menu_get_num_rows_callback(MenuLayer *me, uint16_t section_index, void *data);
static int16_t menu_get_header_height_callback(MenuLayer *me, uint16_t section_index, void *data);
static int16_t menu_get_cell_height_callback(MenuLayer *me, MenuIndex* cell_index, void *data);
static void menu_draw_header_callback(GContext* ctx, const Layer *cell_layer, uint16_t section_index, void *data);
static void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data);
static void menu_draw_line_row(GContext* layer, const Layer* cell_layer, MenuIndex* cell_index);
static void menu_select_click_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context);
static void do_status_request();
static int xatoi (char** str, long* res);
static int NumberOfSetBits(int i);
static TubeLine* get_line_by_code(const char* code);
static TubeLine* get_line_by_pos(int pos);
static void draw_tube_line(GContext* ctx, const Layer* cell_layer, TubeLine* line);
static void draw_tfl_single_line(GContext* ctx, char* text);

static Window window;
static MenuLayer layer_menu;
static HeapBitmap menu_icons[NUM_ICONS];
static GFont fonts[2];
static int state = STATE_UPDATING;
static const char* default_line_order = "BLCECIDIDLHCJLMENOOVPIVIWC";
static bool loaded = false;

static TubeLine lines[] = {
  { "BL\0", 0, "Bakerloo", 0 },
  { "CE\0", 0, "Central", 1 },
  { "CI\0", 0, "Circle", 2 },
  { "DI\0", 0, "District", 3 },
  { "DL\0", 0, "DLR", 4 },
  { "HC\0", 0, "H'smith & City", 5 },
  { "JL\0", 0, "Jubilee", 6 },
  { "ME\0", 0, "Metropolitan", 7 },
  { "NO\0", 0, "Northern", 8 },
  { "OV\0", 0, "Overground", 9 },
  { "PI\0", 0, "Picadilly", 10 },
  { "VI\0", 0, "Victoria", 11 },
  { "WC\0", 0, "Waterloo & City", 12 }
};

/**
 PUBLIC FUNCTIONS
 **/

void wnd_tube_status_init() {
  window_init(&window, "London Tube Window");
  window_set_window_handlers(&window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload
  });

  init_menu(&window);

  fonts[FONT_ROW_HEADER] = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TFL_BOLD_18));
  fonts[FONT_ROW_BODY] = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TFL_15));
}

void wnd_tube_status_show() {
  window_stack_push(&window, true);
}

void wnd_tube_http_failure(int32_t cookie, int http_status, void* context) {
  state = STATE_ERROR;
  menu_layer_reload_data(&layer_menu);
}

void wnd_tube_http_success(int32_t cookie, int http_status, DictionaryIterator* received, void* context) {
  Tuple* tuple_order = dict_find(received, 0);
  Tuple* tuple_statuses = dict_find(received, 1);

  const char* order = tuple_order->value->cstring;
  const char* statuses = tuple_statuses->value->cstring;

  for (int l = 0; l < NUM_LINES; l += 1) {
    char code_str[3];
    long status_num;
    char* status_str = "000";

    strncpy(code_str, order + (l * 2), 2);
    strncpy(status_str, statuses + (l * 3), 3);
    xatoi(&status_str, &status_num);

    TubeLine* line = get_line_by_code(code_str);
    if (! line) {
      vibes_short_pulse();
      continue;
    }
    line->ordering = l;
    line->status = (int)status_num;
  }
  state = STATE_OK;
  menu_layer_reload_data(&layer_menu);
}

/**
 PRIVATE FUNCTIONS
 **/

void window_load(Window* me) {
  load_bitmaps();
  do_status_request();
}

void window_unload(Window* me) {
  unload_bitmaps();
}

void load_bitmaps() {
  heap_bitmap_init(&menu_icons[MENU_ICON_OK], RESOURCE_ID_MENU_OK);
  heap_bitmap_init(&menu_icons[MENU_ICON_PROBLEM], RESOURCE_ID_MENU_PROBLEM);
  heap_bitmap_init(&menu_icons[MENU_ICON_UNKNOWN], RESOURCE_ID_MENU_UNKNOWN);
}

void unload_bitmaps() {
  for (int icon = 0; icon < NUM_ICONS; icon += 1) {
    heap_bitmap_deinit(&menu_icons[icon]);
  }
}

void init_menu(Window* wnd) {
  menu_layer_init(&layer_menu, wnd->layer.bounds);
  menu_layer_set_callbacks(&layer_menu, NULL, (MenuLayerCallbacks){
    .get_num_sections = menu_get_num_sections_callback,
    .get_num_rows = menu_get_num_rows_callback,
    .get_header_height = menu_get_header_height_callback,
    .get_cell_height = menu_get_cell_height_callback,
    .draw_header = menu_draw_header_callback,
    .draw_row = menu_draw_row_callback,
    .select_click = menu_select_click_callback
  });
  menu_layer_set_click_config_onto_window(&layer_menu, wnd);
  layer_add_child(&wnd->layer, menu_layer_get_layer(&layer_menu));
}

void do_status_request() {
  state = STATE_UPDATING;
  menu_layer_reload_data(&layer_menu);

  DictionaryIterator* body;
  HTTPResult result = http_out_get("http://api.pblweb.com/london-tube/v2/status.php", HTTP_TUBE_STATUS, &body);
  if (result != HTTP_OK) {
    state = STATE_ERROR;
    menu_layer_reload_data(&layer_menu);
    return;
  }

  dict_write_cstring(body, 0, default_line_order);
  dict_write_int32(body, 1, 1);

  result = http_out_send();
  if (result != HTTP_OK) {
    state = STATE_ERROR;
    menu_layer_reload_data(&layer_menu);
  }
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
      return 1;
    break;
  }
  return 0;
}

int16_t menu_get_header_height_callback(MenuLayer *me, uint16_t section_index, void *data) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

int16_t menu_get_cell_height_callback(MenuLayer *me, MenuIndex* cell_index, void *data) {
  switch (cell_index->section) {
    case SECTION_LINES:
      return max(40, 24 + (16 * NumberOfSetBits(get_line_by_pos(cell_index->row)->status)));
    break;
    case SECTION_OPTIONS:
      return 40;
    break;
  }
  return 44;
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
    case SECTION_LINES:
      menu_draw_line_row(ctx, cell_layer, cell_index);
    break;
    case SECTION_OPTIONS: {
      switch (cell_index->row) {
        case 0:
          draw_tfl_single_line(ctx, "Refresh Lines");
        break;
      }
    }
  }
}

void menu_draw_line_row(GContext* ctx, const Layer* cell_layer, MenuIndex* cell_index) {
  draw_tube_line(ctx, cell_layer, get_line_by_pos(cell_index->row));
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
      }
    }
    break;
  }
}


void draw_tube_line(GContext* ctx, const Layer* cell_layer, TubeLine* line) {
  char status_label[100] = "";
  GBitmap* bmp;

  strcpy(status_label, "");

  for (int s = 2; s <= 256; s *= 2) {
    if (line->status & s) {
      switch (s) {
        case 2:
          if (strlen(status_label) > 0) {
            strcat(status_label, "\n");
          }
          strcat(status_label, "Minor Delays");
        break;
        case 4:
          if (strlen(status_label) > 0) {
            strcat(status_label, "\n");
          }
          strcat(status_label, "Bus Service");
        break;
        case 8:
          if (strlen(status_label) > 0) {
            strcat(status_label, "\n");
          }
          strcat(status_label, "Reduced Service");
        break;
        case 16:
          if (strlen(status_label) > 0) {
            strcat(status_label, "\n");
          }
          strcat(status_label, "Severe Delays");
        break;
        case 32:
          if (strlen(status_label) > 0) {
            strcat(status_label, "\n");
          }
          strcat(status_label, "Part Closure");
        break;
        case 64:
          if (strlen(status_label) > 0) {
            strcat(status_label, "\n");
          }
          strcat(status_label, "Planned Closure");
        break;
        case 128:
          if (strlen(status_label) > 0) {
            strcat(status_label, "\n");
          }
          strcat(status_label, "Part Suspended");
        break;
        case 256:
          if (strlen(status_label) > 0) {
            strcat(status_label, "\n");
          }
          strcat(status_label, "Suspended");
        break;
      }
    }
  }

  if (strlen(status_label) == 0) {
    switch (line->status) {
      case 0:
        strcpy(status_label, "Getting Status");
        bmp = &menu_icons[MENU_ICON_UNKNOWN].bmp;
      break;
      case 1:
        strcpy(status_label, "Good Service");
        bmp = &menu_icons[MENU_ICON_OK].bmp;
      break;
      default:
        strcpy(status_label, "Unknown Status");
        bmp = &menu_icons[MENU_ICON_UNKNOWN].bmp;
    }
  }
  else {
    bmp = &menu_icons[MENU_ICON_PROBLEM].bmp;
  }

  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_bitmap_in_rect(ctx, bmp, GRect(4, 22, 12, 14));
  graphics_text_draw(ctx, line->name, fonts[FONT_ROW_HEADER], GRect(4, 0, 140, 18), 0, GTextAlignmentLeft, NULL);
  graphics_text_draw(ctx, status_label, fonts[FONT_ROW_BODY], GRect(22, 19, 116, max(18, (18 * NumberOfSetBits(line->status)))), 0, GTextAlignmentLeft, NULL);
}

void draw_tfl_single_line(GContext* ctx, char* text) {
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_text_draw(ctx, text, fonts[FONT_ROW_HEADER], GRect(8, 8, 140, 18), 0, GTextAlignmentLeft, NULL);
}

TubeLine* get_line_by_code(const char* code) {
  for (int l = 0; l < NUM_LINES; l += 1) {
    if (strncmp(lines[l].code, code, 2) == 0) {
      return &lines[l];
    }
  }
  return NULL;
}

TubeLine* get_line_by_pos(int pos) {
  for (int l = 0; l < NUM_LINES; l += 1) {
    if (lines[l].ordering == pos) {
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
int xatoi (char **str, long *res) {
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

int NumberOfSetBits(int i)
{
    i = i - ((i >> 1) & 0x55555555);
    i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
    return (((i + (i >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

