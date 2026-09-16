const std = @import("std");

pub fn build(b: *std.Build) void {
    const optimize = b.standardOptimizeOption(.{});

    const target = b.standardTargetOptions(.{
        .default_target = .{
            .cpu_arch = .x86_64,
            .os_tag = .linux,
            .abi = .gnu,
            .cpu_model = .baseline,
        },
    });

    const exe_mod = b.createModule(.{
        .root_source_file = null,
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });

    exe_mod.addCSourceFiles(.{
        .files = &.{"RockPaperScissors/main.c"},
        .flags = &.{
            "-std=c23",
            "-Wall",
            "-Wextra",
            "-pedantic",
            "-Werror",
            "-Wmissing-prototypes",
            "-Wmissing-variable-declarations",
            "-Wconditional-uninitialized",
            "-Wformat-signedness",
            "-Wdouble-promotion",
            "-Wshadow",
            "-Wswitch-enum",
        },
    });

    exe_mod.addIncludePath(b.path("raylib-5.0_linux_amd64/include"));
    exe_mod.addObjectFile(b.path("raylib-5.0_linux_amd64/lib/libraylib.a"));

    const exe = b.addExecutable(.{
        .name = "RockPaperScissors",
        .root_module = exe_mod,
    });
    b.installArtifact(exe);

    const run = b.addRunArtifact(exe);
    run.setCwd(b.path("."));
    run.step.dependOn(b.getInstallStep());
    b.step("run", "Run it").dependOn(&run.step);
}
