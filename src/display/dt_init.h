// Enum for display initialization results
enum class DisplayInitResult : uint8_t
{
    OK = 0,
    BUS_FAIL,
    RGB_FAIL,
    GFX_ALLOC_FAIL,
    GFX_BEGIN_FAIL,
    I2C_FAIL,
    TOUCH_FAIL,
    LV_BUF_FAIL,
    LV_DISPLAY_FAIL,
    LV_INDEV_FAIL,
    TIMER_FAIL
};

const char *displayInitResultToString(DisplayInitResult r)
{
    switch (r)
    {
    case DisplayInitResult::OK:
        return "OK";
    case DisplayInitResult::BUS_FAIL:
        return "BUS_FAIL";
    case DisplayInitResult::RGB_FAIL:
        return "RGB_FAIL";
    case DisplayInitResult::GFX_ALLOC_FAIL:
        return "GFX_ALLOC_FAIL";
    case DisplayInitResult::GFX_BEGIN_FAIL:
        return "GFX_BEGIN_FAIL";
    case DisplayInitResult::I2C_FAIL:
        return "I2C_FAIL";
    case DisplayInitResult::TOUCH_FAIL:
        return "TOUCH_FAIL";
    case DisplayInitResult::LV_BUF_FAIL:
        return "LV_BUF_FAIL";
    case DisplayInitResult::LV_DISPLAY_FAIL:
        return "LV_DISPLAY_FAIL";
    case DisplayInitResult::LV_INDEV_FAIL:
        return "LV_INDEV_FAIL";
    case DisplayInitResult::TIMER_FAIL:
        return "TIMER_FAIL";
    default:
        return "UNKNOWN";
    }
}
