// Native backend build (C++). Run from native/:  zig build
// Cross-compiles x86_64-windows-gnu. Output: zig-out/bin/
// Targets: dx11-test.exe (test harness), injector.exe (test tool),
// limiter.dll (hook backend).

const std = @import("std");

const cxx_flags = [_][]const u8{ "-std=c++17", "-Wall", "-Wextra" };

fn newCxxModule(b: *std.Build, target: std.Build.ResolvedTarget, optimize: std.builtin.OptimizeMode) *std.Build.Module {
    return b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
}

fn addCxxExecutable(b: *std.Build, target: std.Build.ResolvedTarget, optimize: std.builtin.OptimizeMode, name: []const u8, sources: []const []const u8, syslibs: []const []const u8) *std.Build.Step.Compile {
    const exe = b.addExecutable(.{
        .name = name,
        .root_module = newCxxModule(b, target, optimize),
    });
    for (sources) |src| {
        exe.root_module.addCSourceFile(.{
            .file = b.path(src),
            .flags = &cxx_flags,
        });
    }
    exe.root_module.link_libcpp = true;
    exe.root_module.link_libc = true;
    for (syslibs) |lib| {
        exe.root_module.linkSystemLibrary(lib, .{});
    }
    b.installArtifact(exe);
    return exe;
}

fn addHostTest(b: *std.Build, test_step: *std.Build.Step, optimize: std.builtin.OptimizeMode, name: []const u8, zig_src: []const u8, cpp_src: []const u8) void {
    const t = b.addTest(.{
        .name = name,
        .root_module = b.createModule(.{
            .root_source_file = b.path(zig_src),
            .target = b.graph.host,
            .optimize = optimize,
        }),
    });
    t.root_module.addCSourceFile(.{
        .file = b.path(cpp_src),
        .flags = &cxx_flags,
    });
    t.root_module.link_libcpp = true;
    test_step.dependOn(&b.addRunArtifact(t).step);
}

pub fn build(b: *std.Build) void {
    const target = b.resolveTargetQuery(.{
        .cpu_arch = .x86_64,
        .os_tag = .windows,
        .abi = .gnu,
    });
    const optimize = b.standardOptimizeOption(.{});

    const exe_sources = [_][]const u8{
        "graphics-api/dx11/main.cpp",
        "common/args.cpp",
    };
    // System libs must be listed explicitly.
    const syslibs = [_][]const u8{
        "d3d11",    "dxgi",
        "kernel32", "user32",
        "gdi32",    "ole32",
        "oleaut32", "uuid",
        "advapi32", "shell32",
    };
    const exe = addCxxExecutable(b, target, optimize, "dx11-test", &exe_sources, &syslibs);
    exe.subsystem = .Windows;

    // Test-harness tool, not injected code.
    const injector_sources = [_][]const u8{"injector/main.cpp"};
    const injector_syslibs = [_][]const u8{ "kernel32", "shell32" };
    _ = addCxxExecutable(b, target, optimize, "injector", &injector_sources, &injector_syslibs);

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

    // Host unit tests for pure math (no Windows needed).
    const test_step = b.step("test", "Run host unit tests");
    addHostTest(b, test_step, optimize, "stats-test", "common/stats_test.zig", "common/stats.cpp");
    addHostTest(b, test_step, optimize, "pacer-test", "common/pacer_test.zig", "common/pacer.cpp");
    addHostTest(b, test_step, optimize, "args-test", "common/args_test.zig", "common/args.cpp");
}
