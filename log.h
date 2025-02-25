/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _INIT_LOG_H_
#define _INIT_LOG_H_

#include <stdio.h>

// #define ERROR(x...)   KLOG_ERROR("init", x)
// #define NOTICE(x...)  KLOG_NOTICE("init", x)
// #define INFO(x...)    KLOG_INFO("init", x)


#define ERROR(...)		printf(__VA_ARGS__)
#define RAW(...)		printf(__VA_ARGS__)

#define INFO(...)		ERROR(__VA_ARGS__)
#define NOTICE(...)		ERROR(__VA_ARGS__)

#define LOG_UEVENTS        0  /* log uevent messages if 1. verbose */

#endif
