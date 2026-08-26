#pragma once
#include <lvgl.h>

void gui_init();
void gui_update();
void gui_set_can_status(bool online);

const char *gui_current_page_name();
