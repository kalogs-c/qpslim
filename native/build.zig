// Native backend build (C++). Run from native/:  zig build
// Cross-compiles x86_64-windows-gnu. Output: zig-out/bin/
// Targets: dx11-test.exe (test harness), limiter.dll (hook backend).

const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.resolveTargetQuery(.{
        .cpu_arch = .x86_64,
        .os_tag = .windows,
        .abi = .gnu,
    });
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{
        .name = "dx11-test",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
        }),
    });
    exe.root_module.addCSourceFile(.{
        .file = b.path("dx11-test/main.cpp"),
        .flags = &.{ "-std=c++17", "-Wall", "-Wextra" },
    });
    exe.root_module.link_libcpp = true;
    // System libs must be listed explicitly.
    const syslibs = [_][]const u8{
        "d3d11", "dxgi",
        "kernel32", "user32", "gdi32",
        "ole32", "oleaut32", "uuid",
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
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
        }),
    });
    dll.root_module.addCSourceFile(.{
        .file = b.path("limiter/limiter.cpp"),
        .flags = &.{ "-std=c++17", "-Wall", "-Wextra" },
    });
    // kernel32 + bundled mingw libc (headers + DllMainCRTStartup).
    // No libc++: this DLL uses Win32 API only, by design.
    dll.root_module.link_libc = true;
    dll.root_module.linkSystemLibrary("kernel32", .{});
    b.installArtifact(dll);
}
