#pragma once
// =====================================================================
//  Showreel  --  machine-readable test records on the debug console
// ---------------------------------------------------------------------
//  After tanmatsu-idf6tests' report.c. Each record is one line:
//
//      @@SR-<KIND>@@ <compact json> @@<crc32 hex8>@@
//
//  The CRC covers the JSON and matches Python's zlib.crc32, so
//  tools/testrun.py can drop a line that some other task's log output
//  landed in the middle of. Records never go through ESP_LOG.
// =====================================================================

#include <stddef.h>
#include <stdint.h>

#define REPORT_JSON_MAX 1536

uint32_t report_crc32(void const* data, size_t len);

void report_emit(char const* kind, char const* json);
void report_emitf(char const* kind, char const* fmt, ...) __attribute__((format(printf, 2, 3)));
