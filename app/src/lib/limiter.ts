import { invoke } from "@tauri-apps/api/core";

export const PRESETS = [30, 40, 45, 60, 72, 90, 120];

export interface Stats {
  present_count: number;
  fps_avg: number;
  frametime_avg_ms: number;
  frametime_max_ms: number;
}

export function setTarget(pid: number, fps: number): Promise<void> {
  return invoke("set_target", { pid, fps });
}

export function setEnabled(pid: number, on: boolean): Promise<void> {
  return invoke("set_enabled", { pid, on });
}

export function getStats(pid: number): Promise<Stats> {
  return invoke("get_stats", { pid });
}
