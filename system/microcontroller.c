// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2020-2022 Martin Whitaker.

#include <stdint.h>

#include "usbhcd.h"

#include "microcontroller.h"
#include "config.h"

//------------------------------------------------------------------------------
// Private Variables
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Public Functions
//------------------------------------------------------------------------------

void microcontroller_init(void)
{
	find_usb_microcontroller();
}

