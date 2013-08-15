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
#include "smallstone.h"
#include "config.h"
#include "http.h"
#include "wnd-tube-status.h"
#include "wnd-main-menu.h"

#if ROCKSHOT
#include "rockshot.h"
#endif

// See https://gist.github.com/matthewtole/6144056 for explanation.
#if ANDROID
#define MY_UUID { 0x91, 0x41, 0xB6, 0x28, 0xBC, 0x89, 0x49, 0x8E, 0xB1, 0x47, 0xC8, 0x84, 0xF0, 0x16, 0x02, 0x15 }
#else
#define MY_UUID HTTP_UUID
#endif

PBL_APP_INFO(MY_UUID, "London Transport", "Matthew Tole", VERSION_MAJOR, VERSION_MINOR,  RESOURCE_ID_MENU_ICON, APP_INFO_STANDARD_APP);

static void handle_init(AppContextRef ctx);
static void http_failure(int32_t cookie, int http_status, void* context);
static void http_success(int32_t cookie, int http_status, DictionaryIterator* received, void* context);
static void http_reconnect(void* context);

void pbl_main(void *params) {

  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,
    .messaging_info = {
      .buffer_sizes = {
        .inbound = 256,
        .outbound = 256,
      }
    }
  };

  #if ROCKSHOT
  rockshot_main(&handlers);
  #endif

  app_event_loop(params, &handlers);
}

void handle_init(AppContextRef ctx) {
  http_set_app_id(76782703);

  resource_init_current_app(&APP_RESOURCES);

  wnd_tube_status_init();
  wnd_main_menu_init();

  wnd_main_menu_show();

  create_thanks_window();

  http_register_callbacks((HTTPCallbacks){
    .failure=http_failure,
    .success=http_success
  }, (void*)ctx);

  #if ROCKSHOT
  rockshot_init(ctx);
  #endif
}

void http_failure(int32_t cookie, int http_status, void* context) {
  switch (cookie) {
    case HTTP_TUBE_STATUS:
      wnd_tube_http_failure(cookie, http_status, context);
    break;
  }
}

void http_success(int32_t cookie, int http_status, DictionaryIterator* received, void* context) {
  switch (cookie) {
    case HTTP_TUBE_STATUS:
      wnd_tube_http_success(cookie, http_status, received, context);
    break;
  }
}
