/*
 * Small Stone Apps Utilities
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
#include "http.h"
#include "smallstone.h"

#define HTTP_COOKIE_THANKS 8825

Window window_thanks;
TextLayer layer_text_thanks;
ScrollLayer layer_scroll_thanks;

void create_thanks_window() {

  const GRect max_text_bounds = GRect(4, 4, 128, 2000);
  const int vert_scroll_text_padding = 4;

  window_init(&window_thanks, "Thanks Window");

  scroll_layer_init(&layer_scroll_thanks, window_thanks.layer.frame);
  scroll_layer_set_click_config_onto_window(&layer_scroll_thanks, &window_thanks);

  layer_add_child(&window_thanks.layer, &layer_scroll_thanks.layer);

  static char* message = "By viewing this page you have just sent a nice message to the developer of this app.\
  \nIf you like this app a lot, please consider donating.\
  \nDetails on how to donate can be found at\
  \nhttp://matthewtole.com/pebble/#donate\n\n";
  text_layer_init(&layer_text_thanks, max_text_bounds);
  text_layer_set_text_color(&layer_text_thanks, GColorBlack);
  text_layer_set_background_color(&layer_text_thanks, GColorClear);
  text_layer_set_font(&layer_text_thanks, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text_alignment(&layer_text_thanks, GTextAlignmentLeft);
  text_layer_set_overflow_mode(&layer_text_thanks, GTextOverflowModeWordWrap);
  text_layer_set_text(&layer_text_thanks, message);
  GSize max_size = text_layer_get_max_used_size(app_get_current_graphics_context(), &layer_text_thanks);
  text_layer_set_size(&layer_text_thanks, max_size);
  scroll_layer_add_child(&layer_scroll_thanks, &layer_text_thanks.layer);
  scroll_layer_set_content_size(&layer_scroll_thanks, GSize(144, max_size.h + vert_scroll_text_padding));
}

void show_thanks_window() {
  window_stack_push(&window_thanks, true);
}

void send_thanks(char* app, int ver_maj, int ver_min) {
  DictionaryIterator* body;
  HTTPResult result = http_out_get("http://api.pblweb.com/thanks/v1/thanks.php", HTTP_COOKIE_THANKS, &body);
  if (result != HTTP_OK) {
    return;
  }
  dict_write_cstring(body, 0, app);
  char version_str[10];
  snprintf(version_str, sizeof(version_str), "%d-%d", ver_maj, ver_min);
  dict_write_cstring(body, 1, version_str);
  result = http_out_send();
}