#include "pipe.h"

#include "hook.h"

#include "../common/pipemsg.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>

namespace {

HANDLE g_thread = nullptr;
HANDLE g_stop = nullptr;

void PipeName(char* buf, uint64_t size) {
  snprintf(buf, static_cast<size_t>(size), "\\\\.\\pipe\\qpslim-%lu",
           GetCurrentProcessId());
}

bool WriteReply(HANDLE pipe, const char* reply, int len) {
  if (len < 0) {
    return false;
  }
  DWORD written = 0;
  return WriteFile(pipe, reply, static_cast<DWORD>(len), &written,
                   nullptr) != FALSE;
}

void HandleLine(HANDLE pipe, const char* line) {
  PipeRequest req = {};
  PipeParse(line, &req);
  char reply[512];
  int len = -1;
  if (req.err != PIPE_ERR_NONE) {
    len = PipeFormatErr(reply, sizeof(reply), req.err);
  } else if (req.op == PIPE_OP_SET_FPS) {
    SetTargetFps(static_cast<int>(req.fps));
    len = PipeFormatOk(reply, sizeof(reply));
  } else if (req.op == PIPE_OP_GET_STATS) {
    LimiterStats stats = {};
    if (GetPresentStats(&stats)) {
      len = PipeFormatStats(reply, sizeof(reply), &stats);
    } else {
      len = PipeFormatErr(reply, sizeof(reply), PIPE_ERR_STATS);
    }
  } else {
    len = PipeFormatErr(reply, sizeof(reply), PIPE_ERR_OP);
  }
  WriteReply(pipe, reply, len);
}

void ServeClient(HANDLE pipe) {
  char line[PIPE_MAX_LINE];
  DWORD used = 0;
  bool overflow = false;
  char chunk[256];
  for (;;) {
    DWORD got = 0;
    if (!ReadFile(pipe, chunk, sizeof(chunk), &got, nullptr) || got == 0) {
      return;
    }
    for (DWORD i = 0; i < got; i++) {
      if (chunk[i] == '\n') {
        if (!overflow) {
          line[used] = '\0';
          HandleLine(pipe, line);
        }
        used = 0;
        overflow = false;
      } else if (!overflow) {
        if (used < sizeof(line) - 1) {
          line[used++] = chunk[i];
        } else {
          overflow = true;
        }
      }
    }
  }
}

bool ShouldStop() {
  return WaitForSingleObject(g_stop, 0) == WAIT_OBJECT_0;
}

DWORD WINAPI PipeThread(LPVOID) {
  char name[64];
  PipeName(name, sizeof(name));
  for (;;) {
    HANDLE pipe =
        CreateNamedPipeA(name, PIPE_ACCESS_DUPLEX,
                         PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1,
                         PIPE_MAX_LINE, PIPE_MAX_LINE, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
      return 1;
    }
    BOOL connected = ConnectNamedPipe(pipe, nullptr);
    if (!connected && GetLastError() != ERROR_PIPE_CONNECTED) {
      CloseHandle(pipe);
      if (ShouldStop()) {
        return 0;
      }
      continue;
    }
    ServeClient(pipe);
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
    if (ShouldStop()) {
      return 0;
    }
  }
}

void UnblockAccept() {
  char name[64];
  PipeName(name, sizeof(name));
  HANDLE self = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                            OPEN_EXISTING, 0, nullptr);
  if (self != INVALID_HANDLE_VALUE) {
    CloseHandle(self);
  }
}

}  // namespace

bool Pipe_Start() {
  if (g_thread) {
    return false;
  }
  g_stop = CreateEventA(nullptr, TRUE, FALSE, nullptr);
  if (!g_stop) {
    return false;
  }
  g_thread = CreateThread(nullptr, 0, &PipeThread, nullptr, 0, nullptr);
  if (!g_thread) {
    CloseHandle(g_stop);
    g_stop = nullptr;
    return false;
  }
  SetThreadPriority(g_thread, THREAD_PRIORITY_BELOW_NORMAL);
  return true;
}

void Pipe_Stop() {
  if (!g_thread) {
    return;
  }
  SetEvent(g_stop);
  UnblockAccept();
  WaitForSingleObject(g_thread, 5000);
  CloseHandle(g_thread);
  g_thread = nullptr;
  CloseHandle(g_stop);
  g_stop = nullptr;
}
