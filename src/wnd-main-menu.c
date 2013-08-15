/*
 * London Transport
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
#include "smallstone.h"
#include "wnd-tube-status.h"
#include "wnd-main-menu.h"

#define NUM_ICONS 3
#define ICON_THANKS 0
#define ICON_TUBE 1
#define ICON_BUS 2

static void window_load(Window *me);
static void window_unload(Window *me);
static uint16_t menu_get_num_sections_callback(MenuLayer *me, void *data);
static uint16_t menu_get_num_rows_callback(MenuLayer *me, uint16_t section_index, void *data);
static int16_t menu_get_header_height_callback(MenuLayer *me, uint16_t section_index, void *data);
static int16_t menu_get_cell_height_callback(MenuLayer *me, MenuIndex* cell_index, void *data);
static void menu_draw_header_callback(GContext* ctx, const Layer *cell_layer, uint16_t section_index, void *data);
static void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data);
static void menu_select_click_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context);

static Window window;
static MenuLayer layer_menu;
static HeapBitmap icons[NUM_ICONS];

/**
 PUBLIC FUNCTIONS
 **/

void wnd_main_menu_init() {
  window_init(&window, "London Transport Window");
  window_set_window_handlers(&window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload
  });
}

void wnd_main_menu_show() {
  window_stack_push(&window, true);
}

/**
 PRIVATE FUNCTIONS
 **/

void window_load(Window *me) {
  menu_layer_init(&layer_menu, me->layer.bounds);
  menu_layer_set_callbacks(&layer_menu, NULL, (MenuLayerCallbacks){
    .get_num_sections = menu_get_num_sections_callback,
    .get_num_rows = menu_get_num_rows_callback,
    .get_header_height = menu_get_header_height_callback,
    .get_cell_height = menu_get_cell_height_callback,
    .draw_header = menu_draw_header_callback,
    .draw_row = menu_draw_row_callback,
    .select_click = menu_select_click_callback
  });
  menu_layer_set_click_config_onto_window(&layer_menu, me);
  layer_add_child(&me->layer, menu_layer_get_layer(&layer_menu));

  heap_bitmap_init(&icons[ICON_THANKS], RESOURCE_ID_MENU_ICON_THANKS);
  heap_bitmap_init(&icons[ICON_TUBE], RESOURCE_ID_MENU_ICON_TUBE);
  heap_bitmap_init(&icons[ICON_BUS], RESOURCE_ID_MENU_ICON_BUS);
}

void window_unload(Window *me) {
  heap_bitmap_deinit(&icons[ICON_THANKS]);
  heap_bitmap_deinit(&icons[ICON_TUBE]);
  heap_bitmap_deinit(&icons[ICON_BUS]);
}

uint16_t menu_get_num_sections_callback(MenuLayer *me, void *data) {
  return 1;
}

uint16_t menu_get_num_rows_callback(MenuLayer *me, uint16_t section_index, void *data) {
  return 3;
}

int16_t menu_get_header_height_callback(MenuLayer *me, uint16_t section_index, void *data) {
  return 0;
}

int16_t menu_get_cell_height_callback(MenuLayer *me, MenuIndex* cell_index, void *data) {
  return 40;
}

void menu_draw_header_callback(GContext* ctx, const Layer *cell_layer, uint16_t section_index, void *data) {
  // Do nothing.
}

void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
  char row_text[20];
  HeapBitmap* icon = NULL;
  switch (cell_index->row) {
    case 0:
      strcpy(row_text, "Tube Status");
      icon = &icons[ICON_TUBE];
    break;
    case 1:
      strcpy(row_text, "Next Bus");
      icon = &icons[ICON_BUS];
    break;
    case 2:
      strcpy(row_text, "Thank the Dev");
      icon = &icons[ICON_THANKS];
    break;
  }
  graphics_context_set_text_color(ctx, GColorBlack);
  if (icon) {
    graphics_draw_bitmap_in_rect(ctx, &icon->bmp, GRect(4, 4, 24, 28));
  }
  graphics_text_draw(ctx, row_text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), GRect(32, 2, 108, 26), 0, GTextAlignmentLeft, NULL);
}

void menu_select_click_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context) {
  switch (cell_index->row) {
    case 0:
      wnd_tube_status_show();
    break;
    case 2:
      send_thanks("london-transport", VERSION_MAJOR, VERSION_MINOR);
      show_thanks_window();

  }
}