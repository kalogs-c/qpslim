// injector.exe — loads limiter.dll into a running process via
// CreateRemoteThread + LoadLibraryW, then calls Limiter_HookCreation
// the same way so creation hooks install outside the loader lock.
// Test-harness CLI only:
//   injector.exe <pid> <full-path-to-limiter.dll>
// Same bitness required (x64 -> x64). Never use on anti-cheat games.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cstdlib>
#include <cwchar>

namespace {

constexpr int kUsage = 1;
constexpr int kOpenProcess = 2;
constexpr int kBitness = 3;
constexpr int kAlloc = 4;
constexpr int kInject = 5;
constexpr int kInit = 6;
constexpr DWORD kLoadTimeoutMs = 10000;

void Usage() {
  fprintf(stderr, "usage: injector.exe <pid> <dll-path>\n");
}

void ReportWin32(const char* what) {
  fprintf(stderr, "error: %s failed (%lu)\n", what, GetLastError());
}

bool EnableDebugPrivilege() {
  HANDLE token = nullptr;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES,
                        &token)) {
    return false;
  }
  TOKEN_PRIVILEGES priv = {};
  priv.PrivilegeCount = 1;
  priv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
  bool ok = LookupPrivilegeValueW(nullptr, L"SeDebugPrivilege",
                                  &priv.Privileges[0].Luid) != FALSE;
  if (ok) {
    BOOL adjusted = AdjustTokenPrivileges(token, FALSE, &priv, 0, nullptr,
                                          nullptr);
    ok = adjusted && GetLastError() == ERROR_SUCCESS;
  }
  CloseHandle(token);
  return ok;
}

bool TryIsWow64(HANDLE process, BOOL* out) {
  return IsWow64Process(process, out) != FALSE;
}

bool SameArch(HANDLE process) {
  BOOL self_wow = FALSE;
  BOOL target_wow = FALSE;
  if (!TryIsWow64(GetCurrentProcess(), &self_wow)) {
    return false;
  }
  if (!TryIsWow64(process, &target_wow)) {
    return false;
  }
  return self_wow == target_wow;
}

HANDLE OpenTargetProcess(DWORD pid) {
  return OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION |
                         PROCESS_VM_WRITE | PROCESS_VM_READ |
                         PROCESS_QUERY_INFORMATION,
                     FALSE, pid);
}

int LoadRemoteLibrary(HANDLE process, DWORD pid, LPVOID remote,
                      const wchar_t* dll_path, size_t bytes) {
  if (!WriteProcessMemory(process, remote, dll_path, bytes, nullptr)) {
    ReportWin32("WriteProcessMemory");
    return kInject;
  }
  HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
  auto load_library = reinterpret_cast<LPTHREAD_START_ROUTINE>(
      GetProcAddress(kernel32, "LoadLibraryW"));
  HANDLE thread =
      CreateRemoteThread(process, nullptr, 0, load_library, remote, 0, nullptr);
  if (!thread) {
    ReportWin32("CreateRemoteThread");
    return kInject;
  }
  DWORD wait = WaitForSingleObject(thread, kLoadTimeoutMs);
  DWORD module = 0;
  bool ok = wait == WAIT_OBJECT_0 && GetExitCodeThread(thread, &module) &&
            module != 0;
  CloseHandle(thread);
  if (!ok) {
    ReportWin32("remote LoadLibrary");
    return kInject;
  }
  printf("injected: limiter.dll at 0x%lx in pid %lu\n", module, pid);
  return 0;
}

int InjectLimiter(HANDLE process, DWORD pid, const wchar_t* dll_path) {
  size_t bytes = (wcslen(dll_path) + 1) * sizeof(wchar_t);
  LPVOID remote =
      VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT, PAGE_READWRITE);
  if (!remote) {
    ReportWin32("VirtualAllocEx");
    return kAlloc;
  }
  int rc = LoadRemoteLibrary(process, pid, remote, dll_path, bytes);
  VirtualFreeEx(process, remote, 0, MEM_RELEASE);
  return rc;
}

// Full 64-bit base of our DLL inside the target (exit codes truncate).
uintptr_t FindRemoteBase(DWORD pid, const wchar_t* dll_path) {
  const wchar_t* slash = wcsrchr(dll_path, L'\\');
  const wchar_t* want = slash ? slash + 1 : dll_path;
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
                                         pid);
  if (snap == INVALID_HANDLE_VALUE) {
    return 0;
  }
  uintptr_t base = 0;
  MODULEENTRY32W me = {};
  me.dwSize = sizeof(me);
  if (Module32FirstW(snap, &me)) {
    do {
      const wchar_t* s = wcsrchr(me.szExePath, L'\\');
      const wchar_t* name = s ? s + 1 : me.szExePath;
      if (_wcsicmp(name, want) == 0) {
        base = reinterpret_cast<uintptr_t>(me.modBaseAddr);
        break;
      }
    } while (Module32NextW(snap, &me));
  }
  CloseHandle(snap);
  return base;
}

int CallRemoteInit(HANDLE process, DWORD pid, const wchar_t* dll_path) {
  HMODULE local =
      LoadLibraryExW(dll_path, nullptr, DONT_RESOLVE_DLL_REFERENCES);
  if (!local) {
    ReportWin32("LoadLibraryEx");
    return kInit;
  }
  FARPROC init = GetProcAddress(local, "Limiter_HookCreation");
  uintptr_t rva = init ? reinterpret_cast<uintptr_t>(init) -
                             reinterpret_cast<uintptr_t>(local)
                       : 0;
  FreeLibrary(local);
  if (!init) {
    printf("loaded only: target DLL has no Limiter_HookCreation\n");
    return 0;
  }
  uintptr_t base = FindRemoteBase(pid, dll_path);
  if (!base) {
    fprintf(stderr, "error: remote module not found in pid %lu\n", pid);
    return kInit;
  }
  // x64 ABI: the extra LPVOID is ignored, BOOL return reads as DWORD.
  auto remote_init =
      reinterpret_cast<LPTHREAD_START_ROUTINE>(base + rva);
  HANDLE thread =
      CreateRemoteThread(process, nullptr, 0, remote_init, nullptr, 0, nullptr);
  if (!thread) {
    ReportWin32("CreateRemoteThread(init)");
    return kInit;
  }
  DWORD wait = WaitForSingleObject(thread, kLoadTimeoutMs);
  DWORD ok = 0;
  bool done = wait == WAIT_OBJECT_0 && GetExitCodeThread(thread, &ok) &&
              ok != 0;
  CloseHandle(thread);
  if (!done) {
    fprintf(stderr, "error: remote init failed in pid %lu\n", pid);
    return kInit;
  }
  printf("creation hooks installed in pid %lu\n", pid);
  return 0;
}

} // namespace

int Run(int argc, wchar_t* argv[]) {
  if (argc != 3) {
    Usage();
    return kUsage;
  }
  DWORD pid = static_cast<DWORD>(_wtol(argv[1]));
  if (pid == 0) {
    fprintf(stderr, "error: invalid pid '%ls'\n", argv[1]);
    return kUsage;
  }
  const wchar_t* dll_path = argv[2];

  EnableDebugPrivilege(); // Best effort: needed for elevated targets.

  HANDLE process = OpenTargetProcess(pid);
  if (!process) {
    fprintf(stderr, "error: OpenProcess(%lu) failed (%lu)\n", pid,
            GetLastError());
    return kOpenProcess;
  }
  if (!SameArch(process)) {
    fprintf(stderr, "error: bitness mismatch with pid %lu\n", pid);
    CloseHandle(process);
    return kBitness;
  }
  int rc = InjectLimiter(process, pid, dll_path);
  if (rc == 0) {
    rc = CallRemoteInit(process, pid, dll_path);
  }
  CloseHandle(process);
  return rc;
}

// Plain main (not wmain): Unicode args come from GetCommandLineW instead.
int main() {
  int argc = 0;
  wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (!argv) {
    ReportWin32("CommandLineToArgvW");
    return kUsage;
  }
  int rc = Run(argc, argv);
  LocalFree(argv);
  return rc;
}
