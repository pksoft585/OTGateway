#pragma once

#include <stdint.h>
#include "display/dt_local.h"

// DiplayPage enum
enum class DisplayPage : uint8_t
{
    HEATING = 0,
    DHW,
    DISP,

    COUNT
};

static DisplayPage currentPage = DisplayPage::HEATING;

static inline uint8_t pageIndex(DisplayPage page)
{
    return static_cast<uint8_t>(page);
}

// Runtime values for page
struct PageValues
{
    int arcValue = 0;
    int arcMinValue = 0;
    int arcMaxValue = 1000;

    int currentValue = 0;

    bool enabled = false;
    bool turbo = false;
    bool active = false;
};

// Common values
struct CommonValues
{
    bool wifiConnected = false;
    bool mqttConnected = false;
    bool openthermConnected = false;
    bool flame = false;
    uint32_t unixTime = 0;
    Language language = Language::EN;
};

// Last rendered values
struct LastValues
{
    // Last rendered common values
    CommonValues common;

    // Current runtime values
    CommonValues current;

    // Last rendered page values
    PageValues pages[
        static_cast<uint8_t>(DisplayPage::COUNT)
    ];
};

static PageValues g_pages[
    static_cast<uint8_t>(DisplayPage::COUNT)
];

static LastValues g_last;
