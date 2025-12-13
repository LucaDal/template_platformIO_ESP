#pragma once

#include <Arduino.h>

#ifdef DEBUG
  #define DBG_LOG(tag, msg) Serial.printf(PSTR(tag " %s\n"), (msg))
  #define DBG_LOGF(tag, fmt, ...) Serial.printf(PSTR(tag " " fmt), ##__VA_ARGS__)
#else
  #define DBG_LOG(tag, msg) ((void)0)
  #define DBG_LOGF(tag, ...) ((void)0)
#endif

