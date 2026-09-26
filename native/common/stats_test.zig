// Unit tests for common/stats.h
const std = @import("std");

const LimiterStats = extern struct {
    present_count: u64,
    fps_avg: f64,
    frametime_avg_ms: f64,
    frametime_max_ms: f64,
};

extern fn WindowBounds(count: u64, window_size: u64, first: *u64, n: *u64) callconv(.c) bool;
extern fn StatsCompute(deltas: [*]const u64, n: u64, freq: u64, count: u64, out: *LimiterStats) callconv(.c) bool;

const freq: u64 = 10_000_000;
const window: u64 = 240;
const tick_60: u64 = 166667; // 166667 ticks @10MHz ~= 60fps.

test "empty or invalid input returns false" {
    var out: LimiterStats = undefined;
    var first: u64 = 0;
    var n: u64 = 0;
    try std.testing.expect(!WindowBounds(0, window, &first, &n));
    try std.testing.expect(!WindowBounds(1, window, &first, &n));
    try std.testing.expect(!StatsCompute(&[_]u64{}, 0, freq, 10, &out));
    try std.testing.expect(!StatsCompute(&.{tick_60}, 1, 0, 10, &out));
}

test "WindowBounds slices the recent window" {
    var first: u64 = 0;
    var n: u64 = 0;
    try std.testing.expect(WindowBounds(5, window, &first, &n));
    try std.testing.expectEqual(@as(u64, 2), first);
    try std.testing.expectEqual(@as(u64, 4), n);
    try std.testing.expect(WindowBounds(300, window, &first, &n));
    try std.testing.expectEqual(@as(u64, 61), first);
    try std.testing.expectEqual(@as(u64, 240), n);
}

test "constant 60fps stream" {
    var deltas: [240]u64 = .{tick_60} ** 240;
    var out: LimiterStats = undefined;
    try std.testing.expect(StatsCompute(&deltas, 240, freq, 241, &out));
    try std.testing.expectEqual(@as(u64, 241), out.present_count);
    try std.testing.expectApproxEqAbs(@as(f64, 60.0), out.fps_avg, 0.01);
    try std.testing.expectApproxEqAbs(@as(f64, 16.6667), out.frametime_avg_ms, 0.001);
    try std.testing.expectApproxEqAbs(out.frametime_avg_ms, out.frametime_max_ms, 0.0001);
}

test "single spike moves max, barely moves avg" {
    var deltas: [240]u64 = .{tick_60} ** 240;
    deltas[100] = tick_60 * 5;
    var out: LimiterStats = undefined;
    try std.testing.expect(StatsCompute(&deltas, 240, freq, 241, &out));
    try std.testing.expectApproxEqAbs(@as(f64, 83.3333), out.frametime_max_ms, 0.001);
    try std.testing.expectApproxEqAbs(@as(f64, 16.9444), out.frametime_avg_ms, 0.001);
}
