// Unit tests for common/pacer.h. Pure math, runs on the host:
// zig build test
const std = @import("std");

extern fn SleepMsFor(now: u64, deadline: u64, freq: u64, spin_margin_ticks: u64) callconv(.c) u64;
extern fn AdvanceDeadline(deadline: u64, interval: u64, now: u64) callconv(.c) u64;

const freq: u64 = 10_000_000;
// 166667 ticks @10MHz ~= 60fps; 2ms spin margin = 20000 ticks.
const interval: u64 = 166667;
const margin: u64 = 20000;

test "SleepMsFor sleeps the bulk minus margin" {
    // 16.6ms remaining, 2ms margin -> 14ms truncated.
    try std.testing.expectEqual(@as(u64, 14), SleepMsFor(0, 166667, freq, margin));
}

test "SleepMsFor returns zero when late or inside spin zone" {
    try std.testing.expectEqual(@as(u64, 0), SleepMsFor(200000, 166667, freq, margin));
    try std.testing.expectEqual(@as(u64, 0), SleepMsFor(150000, 166667, freq, margin));
    try std.testing.expectEqual(@as(u64, 0), SleepMsFor(0, 166667, 0, margin));
    try std.testing.expectEqual(@as(u64, 0), SleepMsFor(166667, 166667, freq, margin));
    try std.testing.expectEqual(@as(u64, 0), SleepMsFor(146667, 166667, freq, margin));
}

test "SleepMsFor handles margin edge and truncation" {
    try std.testing.expectEqual(@as(u64, 1), SleepMsFor(0, 30000, freq, margin));
    try std.testing.expectEqual(@as(u64, 0), SleepMsFor(146666, 166667, freq, margin));
    try std.testing.expectEqual(@as(u64, 16), SleepMsFor(0, 166667, freq, 0));
    try std.testing.expectEqual(@as(u64, 0), SleepMsFor(0xFFFFFFFFFFFFFF00, 1000, freq, margin));
}

test "AdvanceDeadline steps one interval normally" {
    try std.testing.expectEqual(@as(u64, 200000), AdvanceDeadline(100000, 100000, 50000));
    try std.testing.expectEqual(@as(u64, 200000), AdvanceDeadline(100000, 100000, 100000));
}

test "AdvanceDeadline skips missed beats without bursting" {
    // 5 intervals late: jumps past now, single step, no debt.
    try std.testing.expectEqual(@as(u64, 600000), AdvanceDeadline(100000, 100000, 550000));
}

test "AdvanceDeadline with zero interval is a no-op" {
    try std.testing.expectEqual(@as(u64, 100000), AdvanceDeadline(100000, 0, 50000));
}

test "AdvanceDeadline lands strictly past now" {
    try std.testing.expectEqual(@as(u64, 300000), AdvanceDeadline(100000, 100000, 200000));
    const next = AdvanceDeadline(100000, 100000, 550000);
    try std.testing.expect(next > 550000);
}
