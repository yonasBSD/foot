#pragma once

#include "wayland.h"

void unicode_mode_activate(struct seat *seat);
void unicode_mode_deactivate(struct seat *seat);
void unicode_mode_updated(struct seat *seat);
