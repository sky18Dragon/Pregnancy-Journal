#include "sticky_app_id.h"

const char *sticky_app_id_name(StickyAppId app)
{
    switch (app) {
    case StickyAppId::Home:
        return "home";
    case StickyAppId::Settings:
        return "settings";
    case StickyAppId::DesktopPet:
        return "desktop_pet";
    case StickyAppId::Pomodoro:
        return "pomodoro";
    case StickyAppId::StatusBoard:
        return "status_board";
    case StickyAppId::BookOfAnswers:
        return "book_of_answers";
    case StickyAppId::Pregnancy:
        return "pregnancy";
    }
    return "unknown";
}

bool sticky_app_id_valid(StickyAppId app)
{
    switch (app) {
    case StickyAppId::Home:
    case StickyAppId::Settings:
    case StickyAppId::DesktopPet:
    case StickyAppId::Pomodoro:
    case StickyAppId::StatusBoard:
    case StickyAppId::BookOfAnswers:
    case StickyAppId::Pregnancy:
        return true;
    }
    return false;
}
