#include "zxcommon.h"
#include <stdio.h>
#include <stdarg.h>
#include <algorithm>

bool isLogEnabled;

void setLoggingEnabled(bool enabled)
{
        isLogEnabled = enabled;
}

bool isLoggingEnabled()
{
        return isLogEnabled;
}
