/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * User interface for devices with no display and no keyboard. The M17
 * configuration is fixed at build time: a target overrides the defaults below
 * through its compile definitions.
 */

#include "core/state.h"
#include "core/ui.h"
#include <string.h>

#ifndef CONFIG_UI_CALLSIGN
#define CONFIG_UI_CALLSIGN "N0CALL"
#endif

#ifndef CONFIG_UI_M17_DEST
#define CONFIG_UI_M17_DEST ""
#endif

#ifndef CONFIG_UI_M17_CAN
#define CONFIG_UI_M17_CAN 0
#endif

/*
 * Copy a string into a fixed-size buffer, truncating it if needed. The buffer
 * is always null-terminated.
 */
static void copyString(char *dst, const char *src, const size_t size)
{
    strncpy(dst, src, size - 1);
    dst[size - 1] = '\0';
}

void ui_init()
{
    copyString(state.settings.callsign, CONFIG_UI_CALLSIGN,
               sizeof(state.settings.callsign));
    copyString(state.settings.m17_dest, CONFIG_UI_M17_DEST,
               sizeof(state.settings.m17_dest));
    state.settings.m17_can = CONFIG_UI_M17_CAN;
}

void ui_drawSplashScreen()
{
}

void ui_saveState()
{
}

void ui_updateFSM(bool *sync_rtx)
{
    // The configuration never changes after boot: the UI thread starts with
    // sync_rtx set, so the rtx is configured once and nothing more is needed.
    (void)sync_rtx;
}

bool ui_updateGUI()
{
    return false;
}

bool ui_pushEvent(const uint8_t type, const uint32_t data)
{
    // There is nothing to react to: drop the event.
    (void)type;
    (void)data;

    return true;
}

void ui_terminate()
{
}
