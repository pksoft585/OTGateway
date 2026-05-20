#pragma once

#include <stddef.h>
#include <stdint.h>

// languages enum
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

// Enum for DisplayText
enum class DisplayText
{
    NONE,
    HEAT,
    TURBO,
    DHW,
    HEAT_ACTION,
    DHW_ACTION,
    BRIGHTNESS,
    TIMEOUT,
    EN,
    CN,
    IT,
    NL,
    RU,
    CZ,
    SK
};

// Localization strings
// EN
static const char *const display_text_en[] =
{
    "",
    "Heat",
    "Turbo",
    "DHW",
    "heating",
    "heating",
    "Brightness",
    "Timeout",
    "EN",
    "CN",
    "IT",
    "NL",
    "RU",
    "CZ",
    "SK"
};

// CN
static const char *const display_text_cn[] =
{
    "",
    "加热",
    "增强",
    "热水",
    "加热中",
    "加热中",
    "亮度",
    "超时",
    "EN",
    "CN",
    "IT",
    "NL",
    "RU",
    "CZ",
    "SK"
};

// IT
static const char *const display_text_it[] =
{
    "",
    "Riscal.",
    "Turbo",
    "ACS",
    "riscaldamento",
    "riscaldamento",
    "Luminosità",
    "Spegnimento",
    "EN",
    "CN",
    "IT",
    "NL",
    "RU",
    "CZ",
    "SK"
};

// NL
static const char *const display_text_nl[] =
{
    "",
    "Warm.",
    "Turbo",
    "SWW",
    "verwarming",
    "verwarming",
    "Helderheid",
    "Time-out",
    "EN",
    "CN",
    "IT",
    "NL",
    "RU",
    "CZ",
    "SK"
};

// RU
static const char *const display_text_ru[] =
{
    "",
    "Отоп.",
    "Турбо",
    "ГВС",
    "отопление",
    "гвс",
    "Яркость",
    "Таймаут",
    "EN",
    "CN",
    "IT",
    "NL",
    "RU",
    "CZ",
    "SK"
};

// SK
static const char *const display_text_sk[] =
{
    "",
    "Kúriť",
    "Turbo",
    "TÚV",
    "kúrenie",
    "ohrev",
    "Jas",
    "Aktívny",
    "EN",
    "CN",
    "IT",
    "NL",
    "RU",
    "CZ",
    "SK"
};

// CZ
static const char *const display_text_cz[] =
{
    "",
    "Topení",
    "Turbo",
    "TUV",
    "topení",
    "ohřev",
    "Jas",
    "Aktivní",
    "EN",
    "CN",
    "IT",
    "NL",
    "RU",
    "CZ",
    "SK"
};

// Localization DisplayText helper
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

// Language to DisplayText helper
static inline DisplayText lt(Language lang)
{
    switch (lang)
    {
        case Language::EN: return DisplayText::EN;
        case Language::CN: return DisplayText::CN;
        case Language::IT: return DisplayText::IT;
        case Language::NL: return DisplayText::NL;
        case Language::RU: return DisplayText::RU;
        case Language::CZ: return DisplayText::CZ;
        case Language::SK: return DisplayText::SK;

        default:
            return DisplayText::EN;
    }
}

// Localization language name helper
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
