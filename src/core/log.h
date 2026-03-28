/**
 * log.h — Temporary logging macros
 *
 * Wraps fprintf(stderr, ...) until a proper logging module is implemented.
 */
#pragma once

#include <stdio.h>

#define LOG_INFO(fmt, ...)  fprintf(stderr, "[INFO]  " fmt "\n", ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)
