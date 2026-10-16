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
const ERR_STATS: c_int = 4;

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

test "rejects non-integer numbers" {
    try std.testing.expectEqual(ERR_FPS, parse("{\"v\":1,\"op\":\"set_fps\",\"fps\":60.5}").err);
    try std.testing.expectEqual(ERR_FPS, parse("{\"v\":1,\"op\":\"set_fps\",\"fps\":-1}").err);
    try std.testing.expectEqual(ERR_VERSION, parse("{\"v\":1.5,\"op\":\"get_stats\"}").err);
    try std.testing.expectEqual(ERR_FPS, parse("{\"v\":1,\"op\":\"set_fps\",\"fps\":4294967296}").err);
}

test "unknown error defaults to bad_op" {
    var buf: [512]u8 = undefined;
    const n = PipeFormatErr(&buf, buf.len, 99);
    try std.testing.expectEqualStrings("{\"v\":1,\"ok\":false,\"err\":\"bad_op\"}", buf[0..@intCast(n)]);
}

const vectors = @embedFile("testdata/pipe_vectors.txt");

fn vector(name: []const u8) []const u8 {
    var lines = std.mem.splitScalar(u8, vectors, '\n');
    while (lines.next()) |raw| {
        const line = std.mem.trim(u8, raw, " \t\r");
        if (line.len == 0 or line[0] == '#') {
            continue;
        }
        const eq = std.mem.indexOfScalar(u8, line, '=') orelse continue;
        if (std.mem.eql(u8, line[0..eq], name)) {
            return line[eq + 1 ..];
        }
    }
    unreachable;
}

fn parseVector(name: []const u8) PipeRequest {
    const json = vector(name);
    var tmp: [512]u8 = undefined;
    std.debug.assert(json.len + 1 < tmp.len);
    @memcpy(tmp[0..json.len], json);
    tmp[json.len] = 0;
    var out: PipeRequest = undefined;
    PipeParse(@ptrCast(&tmp), &out);
    return out;
}

test "shared vectors agree with native parser" {
    const set60 = parseVector("set_fps_60");
    try std.testing.expectEqual(OP_SET_FPS, set60.op);
    try std.testing.expectEqual(@as(u32, 60), set60.fps);
    try std.testing.expectEqual(ERR_NONE, set60.err);
    const set0 = parseVector("set_fps_0");
    try std.testing.expectEqual(@as(u32, 0), set0.fps);
    try std.testing.expectEqual(ERR_NONE, set0.err);
    const setmax = parseVector("set_fps_1000");
    try std.testing.expectEqual(@as(u32, 1000), setmax.fps);
    try std.testing.expectEqual(ERR_NONE, setmax.err);
    const get = parseVector("get_stats");
    try std.testing.expectEqual(OP_GET_STATS, get.op);
    try std.testing.expectEqual(ERR_NONE, get.err);
}

test "shared vectors agree with native formatter" {
    var buf: [512]u8 = undefined;
    var n = PipeFormatOk(&buf, buf.len);
    try std.testing.expectEqualStrings(vector("ok"), buf[0..@intCast(n)]);
    n = PipeFormatErr(&buf, buf.len, ERR_FPS);
    try std.testing.expectEqualStrings(vector("err_fps"), buf[0..@intCast(n)]);
    n = PipeFormatErr(&buf, buf.len, ERR_OP);
    try std.testing.expectEqualStrings(vector("err_op"), buf[0..@intCast(n)]);
    n = PipeFormatErr(&buf, buf.len, ERR_VERSION);
    try std.testing.expectEqualStrings(vector("err_version"), buf[0..@intCast(n)]);
    n = PipeFormatErr(&buf, buf.len, ERR_STATS);
    try std.testing.expectEqualStrings(vector("err_stats"), buf[0..@intCast(n)]);
    var stats = LimiterStats{
        .present_count = 1234,
        .fps_avg = 59.94,
        .frametime_avg_ms = 16.685,
        .frametime_max_ms = 17.031,
    };
    n = PipeFormatStats(&buf, buf.len, &stats);
    try std.testing.expectEqualStrings(vector("stats"), buf[0..@intCast(n)]);
}

test "format rejects empty buffer and reports truncation length" {
    var buf: [512]u8 = undefined;
    try std.testing.expectEqual(@as(c_int, -1), PipeFormatOk(&buf, 0));
    try std.testing.expectEqual(@as(c_int, -1), PipeFormatErr(&buf, 0, ERR_FPS));
    var stats = LimiterStats{
        .present_count = 1,
        .fps_avg = 60.0,
        .frametime_avg_ms = 16.6,
        .frametime_max_ms = 17.0,
    };
    try std.testing.expectEqual(@as(c_int, -1), PipeFormatStats(&buf, 0, &stats));
    var tiny: [8]u8 = undefined;
    const n = PipeFormatOk(&tiny, tiny.len);
    try std.testing.expect(n > @as(c_int, tiny.len));
}

test "format stats matches golden bytes" {
    var stats = LimiterStats{
        .present_count = 1234,
        .fps_avg = 59.94,
        .frametime_avg_ms = 16.685,
        .frametime_max_ms = 17.031,
    };
    var buf: [512]u8 = undefined;
    const n = PipeFormatStats(&buf, buf.len, &stats);
    const text = buf[0..@intCast(n)];
    try std.testing.expectEqualStrings("{\"v\":1,\"ok\":true,\"present_count\":1234,\"fps_avg\":59.940,\"frametime_avg_ms\":16.685,\"frametime_max_ms\":17.031}", text);
}
