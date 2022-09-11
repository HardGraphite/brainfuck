#pragma once

// Record an error message.
const char *hgbf_err_record(const char *fmt, ...);

// Get last error message.
const char *hgbf_err_read(void);

void hgbf_err_cleanup(void);
