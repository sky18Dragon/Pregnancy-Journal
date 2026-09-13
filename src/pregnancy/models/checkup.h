#pragma once

#include <cstdint>

#include "pregnancy_state.h"

constexpr uint8_t kCheckupSchemaVersion = 1U;

struct Checkup {
    uint8_t schema_version = kCheckupSchemaVersion;
    uint32_t id = 0U;
    PregnancyDate date = {};
    uint8_t hour = 9U;
    uint8_t minute = 0U;
    uint8_t recommended_week = 0U;
    bool completed = false;
    bool reference_template = false;
    char title[40] = {};
    char location[48] = {};
    char note[80] = {};
};

bool checkup_valid(const Checkup &checkup);
bool checkup_epoch(const Checkup &checkup, uint32_t &epoch_seconds);
