#ifndef ONIX_FBCON_H
#define ONIX_FBCON_H

#include <onix/types.h>
#include <onix/multiboot2.h>

void fbcon_parse(multi_tag_framebuffer_t *tag);
void fbcon_map();
void fbcon_activate();
bool fbcon_ready();
void fbcon_write(const char *buf, int len);

#endif
