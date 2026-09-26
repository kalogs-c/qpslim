// Native backend build (C++). Run from native/:  zig build
// Cross-compiles x86_64-windows-gnu. Output: zig-out/bin/
// Targets: dx11-test.exe (test harness), limiter.dll (hook backend).

const std = @import("std");

const cxx_flags = [_][]const u8{ "-std=c++17", "-Wall", "-Wextra" };

fn newCxxModule(b: *std.Build, target: std.Build.ResolvedTarget, optimize: std.builtin.OptimizeMode) *std.Build.Module {
    return b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
}

pub fn build(b: *std.Build) void {
    const target = b.resolveTargetQuery(.{
        .cpu_arch = .x86_64,
        .os_tag = .windows,
        .abi = .gnu,
    });
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{
        .name = "dx11-test",
        .root_module = newCxxModule(b, target, optimize),
    });
    exe.root_module.addCSourceFile(.{
        .file = b.path("graphics-api/dx11/main.cpp"),
        .flags = &cxx_flags,
    });
    exe.root_module.link_libcpp = true;
    // System libs must be listed explicitly.
    const syslibs = [_][]const u8{
        "d3d11",    "dxgi",
        "kernel32", "user32",
        "gdi32",    "ole32",
        "oleaut32", "uuid",
        "advapi32", "shell32",
    };
    for (syslibs) |lib| {
        exe.root_module.linkSystemLibrary(lib, .{});
    }
    exe.subsystem = .Windows;
    b.installArtifact(exe);

    const dll = b.addLibrary(.{
        .name = "limiter",
        .linkage = .dynamic,
        .root_module = newCxxModule(b, target, optimize),
    });
    const dll_sources = [_][]const u8{
        "limiter/limiter.cpp",
        "limiter/hook.cpp",
        "common/stats.cpp",
        "common/pacer.cpp",
    };
    for (dll_sources) |src| {
        dll.root_module.addCSourceFile(.{
            .file = b.path(src),
            .flags = &cxx_flags,
        });
    }

    // kernel32 + winmm (timer resolution) + bundled mingw libc.
    // No libc++: this DLL uses Win32 API only, by design.
    dll.root_module.link_libc = true;
    dll.root_module.linkSystemLibrary("kernel32", .{});
    dll.root_module.linkSystemLibrary("winmm", .{});
    b.installArtifact(dll);

    // Host unit tests for the pure stats math (no Windows needed).
    const stats_test = b.addTest(.{
        .name = "stats-test",
        .root_module = b.createModule(.{
            .root_source_file = b.path("common/stats_test.zig"),
            .target = b.graph.host,
            .optimize = optimize,
        }),
    });
    stats_test.root_module.addCSourceFile(.{
        .file = b.path("common/stats.cpp"),
        .flags = &cxx_flags,
    });
    stats_test.root_module.link_libcpp = true;
    const run_stats_test = b.addRunArtifact(stats_test);
    const test_step = b.step("test", "Run host unit tests");
    test_step.dependOn(&run_stats_test.step);

    // Host unit tests for the pure pacer math (no Windows needed).
    const pacer_test = b.addTest(.{
        .name = "pacer-test",
        .root_module = b.createModule(.{
            .root_source_file = b.path("common/pacer_test.zig"),
            .target = b.graph.host,
            .optimize = optimize,
        }),
    });
    pacer_test.root_module.addCSourceFile(.{
        .file = b.path("common/pacer.cpp"),
        .flags = &cxx_flags,
    });
    pacer_test.root_module.link_libcpp = true;
    const run_pacer_test = b.addRunArtifact(pacer_test);
    test_step.dependOn(&run_pacer_test.step);
}
