#pragma once

#include <cstddef>

#include "app_descriptor.h"

const StickyAppDescriptor *sticky_app_registry_data();
size_t sticky_app_registry_count();
const StickyAppDescriptor *sticky_app_registry_find(StickyAppId id);
