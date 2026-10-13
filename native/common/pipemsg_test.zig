// Unit tests for common/pipemsg.h. Pure parsing/formatting, runs on host.
const std = @import("std");

const PipeRequest = extern struct {
    op: c_int,
    fps: u32,
    err: c_int,
};

const LimiterStats = extern struct {
    present_count: u64,
    fps_avg: f64,
    frametime_avg_ms: f64,
    frametime_max_ms: f64,
};

extern fn PipeParse(line: [*:0]const u8, out: *PipeRequest) callconv(.c) void;
extern fn PipeFormatOk(buf: [*]u8, size: u64) callconv(.c) c_int;
extern fn PipeFormatErr(buf: [*]u8, size: u64, err: c_int) callconv(.c) c_int;
extern fn PipeFormatStats(buf: [*]u8, size: u64, stats: *const LimiterStats) callconv(.c) c_int;

const OP_NONE: c_int = 0;
const OP_SET_FPS: c_int = 1;
const OP_GET_STATS: c_int = 2;
const ERR_NONE: c_int = 0;
const ERR_VERSION: c_int = 1;
const ERR_OP: c_int = 2;
const ERR_FPS: c_int = 3;

fn parse(line: [*:0]const u8) PipeRequest {
    var out: PipeRequest = undefined;
    PipeParse(line, &out);
    return out;
}

test "set_fps parses with fps" {
    const req = parse("{\"v\":1,\"op\":\"set_fps\",\"fps\":60}");
    try std.testing.expectEqual(OP_SET_FPS, req.op);
    try std.testing.expectEqual(@as(u32, 60), req.fps);
    try std.testing.expectEqual(ERR_NONE, req.err);
}

test "get_stats parses" {
    const req = parse("{\"v\":1,\"op\":\"get_stats\"}");
    try std.testing.expectEqual(OP_GET_STATS, req.op);
    try std.testing.expectEqual(ERR_NONE, req.err);
}

test "rejects bad version, op and fps" {
    try std.testing.expectEqual(ERR_VERSION, parse("{\"v\":2,\"op\":\"get_stats\"}").err);
    try std.testing.expectEqual(ERR_VERSION, parse("{\"v\":10,\"op\":\"get_stats\"}").err);
    try std.testing.expectEqual(ERR_VERSION, parse("{\"op\":\"get_stats\"}").err);
    try std.testing.expectEqual(ERR_OP, parse("{\"v\":1,\"op\":\"nope\"}").err);
    try std.testing.expectEqual(ERR_OP, parse("{\"v\":1}").err);
    try std.testing.expectEqual(ERR_VERSION, parse("").err);
    try std.testing.expectEqual(ERR_FPS, parse("{\"v\":1,\"op\":\"set_fps\",\"fps\":1001}").err);
    try std.testing.expectEqual(ERR_FPS, parse("{\"v\":1,\"op\":\"set_fps\"}").err);
    try std.testing.expectEqual(ERR_FPS, parse("{\"v\":1,\"op\":\"set_fps\",\"fps\":\"x\"}").err);
}

test "fps zero and max pass" {
    try std.testing.expectEqual(@as(u32, 0), parse("{\"v\":1,\"op\":\"set_fps\",\"fps\":0}").fps);
    try std.testing.expectEqual(@as(u32, 1000), parse("{\"v\":1,\"op\":\"set_fps\",\"fps\":1000}").fps);
}

test "format ok and errors match golden bytes" {
    var buf: [512]u8 = undefined;
    var n = PipeFormatOk(&buf, buf.len);
    try std.testing.expectEqualStrings("{\"v\":1,\"ok\":true}", buf[0..@intCast(n)]);
    n = PipeFormatErr(&buf, buf.len, ERR_FPS);
    try std.testing.expectEqualStrings("{\"v\":1,\"ok\":false,\"err\":\"bad_fps\"}", buf[0..@intCast(n)]);
    n = PipeFormatErr(&buf, buf.len, ERR_VERSION);
    try std.testing.expectEqualStrings("{\"v\":1,\"ok\":false,\"err\":\"bad_version\"}", buf[0..@intCast(n)]);
    n = PipeFormatErr(&buf, buf.len, ERR_OP);
    try std.testing.expectEqualStrings("{\"v\":1,\"ok\":false,\"err\":\"bad_op\"}", buf[0..@intCast(n)]);
}

test "format stats carries the counters" {
    var stats = LimiterStats{
        .present_count = 1234,
        .fps_avg = 59.94,
        .frametime_avg_ms = 16.685,
        .frametime_max_ms = 17.031,
    };
    var buf: [512]u8 = undefined;
    const n = PipeFormatStats(&buf, buf.len, &stats);
    const text = buf[0..@intCast(n)];
    try std.testing.expect(std.mem.indexOf(u8, text, "\"present_count\":1234") != null);
    try std.testing.expect(std.mem.indexOf(u8, text, "\"ok\":true") != null);
}
