#pragma once

#include <stdint.h>

#include "stats.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PIPE_MAX_LINE 4096

#define PIPE_OP_NONE 0
#define PIPE_OP_SET_FPS 1
#define PIPE_OP_GET_STATS 2

#define PIPE_ERR_NONE 0
#define PIPE_ERR_VERSION 1
#define PIPE_ERR_OP 2
#define PIPE_ERR_FPS 3
#define PIPE_ERR_STATS 4

struct PipeRequest {
  int op;
  uint32_t fps;
  int err;
};

void PipeParse(const char* line, PipeRequest* out);
int PipeFormatOk(char* buf, uint64_t size);
int PipeFormatErr(char* buf, uint64_t size, int err);
int PipeFormatStats(char* buf, uint64_t size, const LimiterStats* stats);

#ifdef __cplusplus
}
#endif
