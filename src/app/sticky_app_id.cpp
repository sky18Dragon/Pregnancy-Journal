#include "sticky_app_id.h"

const char *sticky_app_id_name(StickyAppId app)
{
    switch (app) {
    case StickyAppId::Home:
        return "home";
    case StickyAppId::Settings:
        return "settings";
    }
    return "unknown";
}

bool sticky_app_id_valid(StickyAppId app)
{
    switch (app) {
    case StickyAppId::Home:
    case StickyAppId::Settings:
        return true;
    }
    return false;
}
