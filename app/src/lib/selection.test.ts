import { expect, test } from "vitest";
import { PRESETS } from "./limiter";
import { parsePid, stepIndex } from "./selection";

test("parsePid accepts positive integers only", () => {
  expect(parsePid("1234")).toBe(1234);
  expect(parsePid("")).toBeNull();
  expect(parsePid("abc")).toBeNull();
  expect(parsePid("0")).toBeNull();
  expect(parsePid("-5")).toBeNull();
  expect(parsePid("12x")).toBe(12);
});

test("stepIndex wraps around both ends", () => {
  const n = PRESETS.length;
  expect(stepIndex(0, -1, n)).toBe(n - 1);
  expect(stepIndex(n - 1, 1, n)).toBe(0);
  expect(stepIndex(3, 1, n)).toBe(4);
  expect(stepIndex(3, -1, n)).toBe(2);
});
