//! Capture effect: a puff of debris plus a shockwave ring, thrown off at the
//! moment one entity takes another. Both pools live here; the C side only
//! spawns bursts and ticks the system once per frame.

const std = @import("std");

const PARTICLE_COUNT = 3072; // shared pool, oldest simply run out
const BURST_PARTICLES = 34; // spawned per capture
const PARTICLE_LIFE = 0.85;
const PARTICLE_SPEED = 440.0;
const PARTICLE_DRAG = 2.2;
const PARTICLE_SIZE = 11.0;

const SHOCK_COUNT = 96;
const SHOCK_LIFE = 0.28;
const SHOCK_RADIUS = 110.0;
const SHOCK_THICKNESS = 9.0;

/// Mirrors raylib's Vector2 and Color layouts. Declared by hand rather than
/// translated, so the library needs nothing but the raylib archive at link time.
pub const Vector2 = extern struct { x: f32, y: f32 };
pub const Color = extern struct { r: u8, g: u8, b: u8, a: u8 };

extern fn DrawCircleV(center: Vector2, radius: f32, color: Color) void;
extern fn DrawRing(
    center: Vector2,
    innerRadius: f32,
    outerRadius: f32,
    startAngle: f32,
    endAngle: f32,
    segments: c_int,
    color: Color,
) void;

const Particle = struct {
    position: Vector2,
    velocity: Vector2,
    life: f32, // seconds remaining, <=0 means the slot is free
    color: Color,
};

const Shockwave = struct {
    position: Vector2,
    life: f32,
    color: Color,
};

var particles: [PARTICLE_COUNT]Particle = @splat(std.mem.zeroes(Particle));
var shockwaves: [SHOCK_COUNT]Shockwave = @splat(std.mem.zeroes(Shockwave));
var prng: std.Random.DefaultPrng = undefined;

fn randUnit() f32 {
    return prng.random().float(f32);
}

/// Frees every slot and seeds the effect randomness. Call before the first
/// spawn and again whenever the round restarts.
export fn psReset(seed: u64) void {
    prng = .init(seed);

    for (&particles) |*p| {
        p.life = 0.0;
    }
    for (&shockwaves) |*s| {
        s.life = 0.0;
    }
}

/// Scatter debris from a capture. Silently does nothing if the pool is full,
/// which only happens during a pile-up and is not worth handling.
export fn psSpawnCaptureBurst(x: f32, y: f32, color: Color) void {
    var spawned: u32 = 0;

    for (&particles) |*p| {
        if (spawned >= BURST_PARTICLES) break;
        if (p.life > 0.0) continue;

        const angle = randUnit() * 2.0 * std.math.pi;
        const speed = PARTICLE_SPEED * (0.35 + 0.65 * randUnit());

        p.position = .{ .x = x, .y = y };
        p.velocity = .{ .x = @cos(angle) * speed, .y = @sin(angle) * speed };
        p.life = PARTICLE_LIFE * (0.7 + 0.3 * randUnit());
        p.color = color;
        spawned += 1;
    }

    for (&shockwaves) |*s| {
        if (s.life > 0.0) continue;

        s.position = .{ .x = x, .y = y };
        s.life = SHOCK_LIFE;
        s.color = color;
        break;
    }
}

export fn psUpdate(dt: f32) void {
    for (&particles) |*p| {
        if (p.life <= 0.0) continue;

        p.life -= dt;
        p.position.x += p.velocity.x * dt;
        p.position.y += p.velocity.y * dt;

        const drag = @max(0.0, 1.0 - PARTICLE_DRAG * dt);
        p.velocity.x *= drag;
        p.velocity.y *= drag;
    }

    for (&shockwaves) |*s| {
        if (s.life > 0.0) {
            s.life -= dt;
        }
    }
}

/// Draws rings first so debris stays on top. Must run inside BeginDrawing().
export fn psDraw() void {
    for (&shockwaves) |*s| {
        if (s.life <= 0.0) continue;

        // Expands outward as it dies, thinning and fading as it goes.
        const s01 = s.life / SHOCK_LIFE;
        const grow = 1.0 - s01;

        const outer = SHOCK_RADIUS * grow;
        const inner = @max(0.0, outer - SHOCK_THICKNESS * s01);

        var sc = s.color;
        sc.a = @intFromFloat(230.0 * s01);

        DrawRing(s.position, inner, outer, 0.0, 360.0, 32, sc);
    }

    for (&particles) |*p| {
        if (p.life <= 0.0) continue;

        // Shrink and fade together over the particle's remaining life.
        const t01 = @min(1.0, p.life / PARTICLE_LIFE);

        // Flash white at the instant of impact, cooling to the type colour.
        const heat = t01 * t01 * t01;
        const base = p.color;

        const pc: Color = .{
            .r = @intFromFloat(@as(f32, @floatFromInt(base.r)) + (255.0 - @as(f32, @floatFromInt(base.r))) * heat),
            .g = @intFromFloat(@as(f32, @floatFromInt(base.g)) + (255.0 - @as(f32, @floatFromInt(base.g))) * heat),
            .b = @intFromFloat(@as(f32, @floatFromInt(base.b)) + (255.0 - @as(f32, @floatFromInt(base.b))) * heat),
            .a = @intFromFloat(255.0 * t01),
        };

        DrawCircleV(p.position, PARTICLE_SIZE * (0.30 + 0.70 * t01), pc);
    }
}
