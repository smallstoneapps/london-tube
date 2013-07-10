#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"
#include "http.h"

#define MY_UUID { 0x91, 0x41, 0xB6, 0x28, 0xBC, 0x89, 0x49, 0x8E, 0xB1, 0x47, 0xC8, 0x84, 0xF0, 0x16, 0x02, 0x15 }

PBL_APP_INFO(MY_UUID, "London Tube", "Matthew Tole", 1, 0,  RESOURCE_ID_MENU_ICON, APP_INFO_STANDARD_APP);

struct Line {
  char* name,
  char* status
} TubeLine;

static const char* lines[] = {
  "Bakerloo", "Central", "Circle", "District", "DLR", "H'smith & City",
  "Jubilee", "Metropolitan", "Northern", "Overground", "Picadilly",
  "Victoria", "Waterloo & City"
};
#define NUM_LINES 13

Window window;
MenuLayer layer_menu;

HeapBitmap menu_icons[3];
#define MENU_ICON_OK 0
#define MENU_ICON_PROBLEM 1
#define MENU_ICON_UNKNOWN 2

#define WEATHER_HTTP_COOKIE 1949327671

void handle_init(AppContextRef ctx);
void window_load(Window *me);
void window_unload(Window *me);
uint16_t menu_get_num_sections_callback(MenuLayer *me, void *data);
uint16_t menu_get_num_rows_callback(MenuLayer *me, uint16_t section_index, void *data);
int16_t menu_get_header_height_callback(MenuLayer *me, uint16_t section_index, void *data);
void menu_draw_header_callback(GContext* ctx, const Layer *cell_layer, uint16_t section_index, void *data);
void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data);

void http_reconnect(void* context);
void http_location(float latitude, float longitude, float altitude, float accuracy, void* context);
void http_failed(int32_t cookie, int http_status, void* context);
void http_success(int32_t cookie, int http_status, DictionaryIterator* received, void* context);

void pbl_main(void *params) {

  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,
    .messaging_info = {
      .buffer_sizes = {
        .inbound = 124,
        .outbound = 256,
      }
    }
  };
  app_event_loop(params, &handlers);

}

void handle_init(AppContextRef ctx) {

  resource_init_current_app(&APP_RESOURCES);

  window_init(&window, "London Tube Window");
  window_stack_push(&window, true);

  http_register_callbacks((HTTPCallbacks){
    .failure=http_failed,
    .success=http_success,
    .reconnect=http_reconnect,
    .location=http_location
  }, (void*)ctx);

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
    // .select_click = menu_select_callback,
  });

  menu_layer_set_click_config_onto_window(&layer_menu, me);

  layer_add_child(&me->layer, menu_layer_get_layer(&layer_menu));

  DictionaryIterator *body;
  HTTPResult result = http_out_get("http://requestb.in/1adgiyz1", WEATHER_HTTP_COOKIE, &body);

  dict_write_int32(body, 0, 1);
  http_out_send();

}

void window_unload(Window* me) {

  heap_bitmap_deinit(&menu_icons[0]);
  heap_bitmap_deinit(&menu_icons[1]);
  heap_bitmap_deinit(&menu_icons[2]);

}

uint16_t menu_get_num_sections_callback(MenuLayer *me, void *data) {
  return 1;
}

uint16_t menu_get_num_rows_callback(MenuLayer *me, uint16_t section_index, void *data) {
  return NUM_LINES;
}

int16_t menu_get_header_height_callback(MenuLayer *me, uint16_t section_index, void *data) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

void menu_draw_header_callback(GContext* ctx, const Layer *cell_layer, uint16_t section_index, void *data) {
   menu_cell_basic_header_draw(ctx, cell_layer, NULL);
}

void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
  switch (cell_index->section) {
    case 0: {
      if (cell_index->row < 13) {
        menu_cell_basic_draw(ctx, cell_layer, lines[cell_index->row], "Status unknown", &menu_icons[MENU_ICON_UNKNOWN].bmp);
      }
      else {
        menu_cell_basic_draw(ctx, cell_layer, NULL, NULL, NULL);
      }
    }
    break;
  }
}

void http_reconnect(void* context) {
}

void http_location(float latitude, float longitude, float altitude, float accuracy, void* context) {
}

void http_failed(int32_t cookie, int http_status, void* context) {
}

void http_success(int32_t cookie, int http_status, DictionaryIterator* received, void* context) {
}