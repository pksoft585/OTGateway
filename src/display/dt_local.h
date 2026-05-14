#pragma once

#include <stddef.h>
#include <stdint.h>

// Enum for language selection
enum class Language : uint8_t
{
    EN = 0,
    CN = 1,
    IT = 2,
    NL = 3,
    RU = 4,
    CZ = 5,
    SK = 6
};

// Enum for display text identifiers
enum class DisplayText
{
    HEAT,
    TURBO,
    DHW,
    HEAT_ACTION,
    DHW_ACTION,
    BRIGHTNESS,
    TIMEOUT
};

// Localization strings
// EN
static const char *const display_text_en[] =
{
    "Heat",
    "Turbo",
    "DHW",
    "heating",
    "heating",
    "Brightness",
    "Timeout"
};

// CN
static const char *const display_text_cn[] =
{
    "加热",
    "增强",
    "热水",
    "加热中",
    "加热中",
    "亮度",
    "超时"
};

// IT
static const char *const display_text_it[] =
{
    "Riscal.",
    "Turbo",
    "ACS",
    "riscaldamento",
    "riscaldamento",
    "Luminosità",
    "Spegnimento"
};

// NL
static const char *const display_text_nl[] =
{
    "Warm.",
    "Turbo",
    "SWW",
    "verwarming",
    "verwarming",
    "Helderheid",
    "Time-out"
};

// RU
static const char *const display_text_ru[] =
{
    "Отоп.",
    "Турбо",
    "ГВС",
    "отопление",
    "гвс",
    "Яркость",
    "Таймаут"
};

// SK
static const char *const display_text_sk[] =
{
    "Kúriť",
    "Turbo",
    "TÚV",
    "kúrenie",
    "ohrev",
    "Jas",
    "Aktívny"
};

// CZ
static const char *const display_text_cz[] =
{
    "Topení",
    "Turbo",
    "TUV",
    "topení",
    "ohřev",
    "Jas",
    "Aktivní"
};

// Language display text
static inline const char *tr(Language lang, DisplayText id)
{
    size_t idx = static_cast<size_t>(id);
    switch (lang)
    {
        case Language::CN:
            return display_text_cn[idx];

        case Language::IT:
            return display_text_it[idx];

        case Language::NL:
            return display_text_nl[idx];

        case Language::RU:
            return display_text_ru[idx];

        case Language::CZ:
            return display_text_cz[idx];

        case Language::SK:
            return display_text_sk[idx];

        case Language::EN:
        default:
            return display_text_en[idx];
    }
}

// Language helper
static inline const char *languageShort(Language lang)
{
    switch (lang)
    {
        case Language::EN: return "EN";
        case Language::CN: return "CN";
        case Language::IT: return "IT";
        case Language::NL: return "NL";
        case Language::RU: return "RU";
        case Language::CZ: return "CZ";
        case Language::SK: return "SK";

        default:
            return "--";
    }
}
