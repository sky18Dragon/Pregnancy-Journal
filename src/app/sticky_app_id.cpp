#include "sticky_app_id.h"

const char *sticky_app_id_name(StickyAppId app) {
  switch (app) {
  case StickyAppId::Settings:
    return "settings";
  case StickyAppId::Pregnancy:
    return "pregnancy";
  case StickyAppId::Weight:
    return "weight";
  case StickyAppId::Kicks:
    return "kicks";
  }
  return "unknown";
}

bool sticky_app_id_valid(StickyAppId app) {
  switch (app) {
  case StickyAppId::Settings:
  case StickyAppId::Pregnancy:
  case StickyAppId::Weight:
  case StickyAppId::Kicks:
    return true;
  }
  return false;
}
