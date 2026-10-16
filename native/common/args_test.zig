// Unit tests for common/args.h. Pure parsing, runs on the host:
// zig build test
const std = @import("std");

extern fn ParseTargetFps(arg: ?[*:0]const u8, fallback: c_int) callconv(.c) c_int;

test "valid values pass through" {
    try std.testing.expectEqual(@as(c_int, 60), ParseTargetFps("60", 60));
    try std.testing.expectEqual(@as(c_int, 1), ParseTargetFps("1", 60));
    try std.testing.expectEqual(@as(c_int, 1000), ParseTargetFps("1000", 60));
}

test "garbage falls back" {
    try std.testing.expectEqual(@as(c_int, 60), ParseTargetFps("", 60));
    try std.testing.expectEqual(@as(c_int, 60), ParseTargetFps("abc", 60));
    try std.testing.expectEqual(@as(c_int, 60), ParseTargetFps("60fps", 60));
    try std.testing.expectEqual(@as(c_int, 60), ParseTargetFps("0", 60));
    try std.testing.expectEqual(@as(c_int, 60), ParseTargetFps("-30", 60));
    try std.testing.expectEqual(@as(c_int, 60), ParseTargetFps("1001", 60));
    try std.testing.expectEqual(@as(c_int, 60), ParseTargetFps(null, 60));
}

test "fallback is honored, not hardcoded" {
    try std.testing.expectEqual(@as(c_int, 144), ParseTargetFps("abc", 144));
    try std.testing.expectEqual(@as(c_int, 30), ParseTargetFps(null, 30));
    try std.testing.expectEqual(@as(c_int, 90), ParseTargetFps("", 90));
    try std.testing.expectEqual(@as(c_int, 72), ParseTargetFps("1001", 72));
    try std.testing.expectEqual(@as(c_int, 45), ParseTargetFps("0", 45));
}

test "strtol tolerance is locked" {
    try std.testing.expectEqual(@as(c_int, 60), ParseTargetFps(" 60", 60));
    try std.testing.expectEqual(@as(c_int, 60), ParseTargetFps("+60", 60));
    try std.testing.expectEqual(@as(c_int, 60), ParseTargetFps("060", 60));
}

test "trailing garbage and overflow fall back" {
    try std.testing.expectEqual(@as(c_int, 60), ParseTargetFps("60 ", 60));
    try std.testing.expectEqual(@as(c_int, 60), ParseTargetFps("60.0", 60));
    try std.testing.expectEqual(@as(c_int, 60), ParseTargetFps("0x3C", 60));
    try std.testing.expectEqual(@as(c_int, 60), ParseTargetFps("99999999999999999999", 60));
    try std.testing.expectEqual(@as(c_int, 60), ParseTargetFps("2147483648", 60));
}
