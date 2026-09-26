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
