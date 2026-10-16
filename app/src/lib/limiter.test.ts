import { afterEach, expect, test } from "vitest";
import { clearMocks, mockIPC } from "@tauri-apps/api/mocks";
import { getStats, setEnabled, setTarget, type Stats } from "./limiter";

afterEach(() => {
  clearMocks();
});

function capture() {
  const calls: Array<{ cmd: string; args: unknown }> = [];
  mockIPC((cmd, args) => {
    calls.push({ cmd, args });
  });
  return calls;
}

test("setTarget invokes set_target with pid and fps", async () => {
  const calls = capture();
  await setTarget(1234, 60);
  expect(calls).toEqual([{ cmd: "set_target", args: { pid: 1234, fps: 60 } }]);
});

test("setEnabled invokes set_enabled with pid and flag", async () => {
  const calls = capture();
  await setEnabled(1234, false);
  expect(calls).toEqual([{ cmd: "set_enabled", args: { pid: 1234, on: false } }]);
});

test("getStats invokes get_stats and passes the reply through", async () => {
  const reply: Stats = {
    present_count: 120,
    fps_avg: 59.9,
    frametime_avg_ms: 16.7,
    frametime_max_ms: 17.1
  };
  mockIPC((cmd) => {
    if (cmd === "get_stats") {
      return reply;
    }
  });
  await expect(getStats(1234)).resolves.toEqual(reply);
});

test("backend errors reject the wrappers", async () => {
  mockIPC(() => {
    throw new Error("pipe io: connection refused");
  });
  await expect(setTarget(1234, 60)).rejects.toThrow("pipe io");
  await expect(setEnabled(1234, true)).rejects.toThrow("pipe io");
  await expect(getStats(1234)).rejects.toThrow("pipe io");
});
