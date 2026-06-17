/********************************************************************************
 * Copyright (c) 2026 Slick Quant LLC
 * All rights reserved
 *
 * This file is part of the slick-dynamic-buffer. Redistribution and use in source
 * and binary forms, with or without modification, are permitted exclusively
 * under the terms of the MIT license which is available at
 * https://github.com/SlickQuant/slick-dynamic-buffer/blob/main/LICENSE
 *
 ********************************************************************************/

#pragma once

// Backward-compatibility shim. The canonical header is <slick/dynamic_buffer.hpp>.
#pragma once
#ifdef _MSC_VER
#  pragma message("warning: <slick/dynamic_buffer.h> is deprecated; use <slick/dynamic_buffer.hpp>")
#else
#  warning "<slick/dynamic_buffer.h> is deprecated; use <slick/dynamic_buffer.hpp>"
#endif
#include "dynamic_buffer.hpp"
