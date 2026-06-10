#pragma once

#include <LeanTask.h>
#include "dt_init.h"

// DisplayTask Interval & Task priority
#define DISPLAYTASK_INTERVAL 33
const UBaseType_t ACTIVE_PRIORITY = 3;
const UBaseType_t SLEEP_PRIORITY  = 3;

// Display initialization
DisplayInitResult display_init()
{
    return DisplayInitResult::OK;
}

