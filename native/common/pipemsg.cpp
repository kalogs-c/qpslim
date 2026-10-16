#include "pipemsg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "args.h"

namespace {

const char* ErrText(int err) {
  switch (err) {
    case PIPE_ERR_VERSION:
      return "bad_version";
    case PIPE_ERR_FPS:
      return "bad_fps";
    case PIPE_ERR_STATS:
      return "no_stats";
    default:
      return "bad_op";
  }
}

const char* AfterKey(const char* line, const char* key) {
  const char* at = strstr(line, key);
  if (!at) {
    return nullptr;
  }
  return at + strlen(key);
}

void Fail(PipeRequest* out, int err) {
  PipeRequest req = {PIPE_OP_NONE, 0, err};
  *out = req;
}

bool IsNumEnd(char c) {
  return c == '\0' || c == ',' || c == '}' || c == ' ' || c == '\t' ||
         c == '\r' || c == '\n';
}

bool TakeVersion(const char* line, bool* ok) {
  const char* at = AfterKey(line, "\"v\":");
  if (!at) {
    return false;
  }
  char* end = nullptr;
  unsigned long v = strtoul(at, &end, 10);
  *ok = (end != at && v == 1 && IsNumEnd(*end));
  return true;
}

bool TakeFps(const char* line, uint32_t* fps) {
  const char* at = AfterKey(line, "\"fps\":");
  if (!at) {
    return false;
  }
  char* end = nullptr;
  unsigned long v = strtoul(at, &end, 10);
  if (end == at || !IsNumEnd(*end) || v > FPS_MAX) {
    return false;
  }
  *fps = static_cast<uint32_t>(v);
  return true;
}

}  // namespace

void PipeParse(const char* line, PipeRequest* out) {
  PipeRequest bad = {PIPE_OP_NONE, 0, PIPE_ERR_OP};
  if (!line || !out) {
    if (out) {
      *out = bad;
    }
    return;
  }
  bool version_ok = false;
  if (!TakeVersion(line, &version_ok) || !version_ok) {
    Fail(out, PIPE_ERR_VERSION);
    return;
  }
  if (strstr(line, "\"op\":\"set_fps\"")) {
    uint32_t fps = 0;
    if (!TakeFps(line, &fps)) {
      Fail(out, PIPE_ERR_FPS);
      return;
    }
    PipeRequest req = {PIPE_OP_SET_FPS, fps, PIPE_ERR_NONE};
    *out = req;
    return;
  }
  if (strstr(line, "\"op\":\"get_stats\"")) {
    PipeRequest req = {PIPE_OP_GET_STATS, 0, PIPE_ERR_NONE};
    *out = req;
    return;
  }
  *out = bad;
}

int PipeFormatOk(char* buf, uint64_t size) {
  if (!buf || size == 0) {
    return -1;
  }
  return snprintf(buf, static_cast<size_t>(size), "{\"v\":1,\"ok\":true}");
}

int PipeFormatErr(char* buf, uint64_t size, int err) {
  if (!buf || size == 0) {
    return -1;
  }
  return snprintf(buf, static_cast<size_t>(size),
                  "{\"v\":1,\"ok\":false,\"err\":\"%s\"}", ErrText(err));
}

int PipeFormatStats(char* buf, uint64_t size, const LimiterStats* stats) {
  if (!buf || size == 0 || !stats) {
    return -1;
  }
  return snprintf(buf, static_cast<size_t>(size),
                  "{\"v\":1,\"ok\":true,\"present_count\":%llu,"
                  "\"fps_avg\":%.3f,\"frametime_avg_ms\":%.3f,"
                  "\"frametime_max_ms\":%.3f}",
                  static_cast<unsigned long long>(stats->present_count),
                  stats->fps_avg, stats->frametime_avg_ms,
                  stats->frametime_max_ms);
}
