// Headless tests for base and phys. Built to its own wasm and run in node (see test.sh).
#include "base/base.h"
#include "base/math.c"
#include "base/rand.c"
#include "base/str.c"
#include "phys/phys.h"
#include "phys/particle.c"
#include "phys/collide.c"
#include "phys/force.c"

__attribute__((import_module("env"), import_name("t_fail"))) void t_fail(const char *expr, i32 len, i32 line);

static i32 failures;

#define CHECK(cond) do { if (!(cond)) { failures++; t_fail(#cond, str_len(#cond), __LINE__); } } while (0)
#define NEAR(a, b) CHECK(near((a), (b), 0.001f))

static i32 near(f32 a, f32 b, f32 eps) {
    f32 d = a > b ? a - b : b - a;
    return d < eps;
}

static void test_vectors(void) {
    V3 a = v3_create(1, 2, 3), b = v3_create(4, 5, 6);
    NEAR(v3_scalar_product(a, b), 32);   // cphiz got 20 here, it used + for y and z
    V3 cross = v3_vector_product(a, b);
    NEAR(cross.x, -3);
    NEAR(cross.y, 6);
    NEAR(cross.z, -3);
    NEAR(v3_scalar_product(cross, a), 0); // cross product is perpendicular to both
    NEAR(v3_scalar_product(cross, b), 0);
    V3 z = v3_vector_product(v3_create(1, 0, 0), v3_create(0, 1, 0));
    NEAR(z.z, 1);                        // x cross y = z (right handed)
    V3 comp = v3_component_product(a, b);
    NEAR(comp.x, 4);
    NEAR(comp.y, 10);
    NEAR(comp.z, 18);
    NEAR(v3_magnitude(v3_create(2, 3, 6)), 7);
    NEAR(v3_added(a, b).z, 9);
    NEAR(v3_subtracted(a, b).x, -3);
    NEAR(v3_multiplied(a, 3).z, 9);
    V3 c = a;
    v3_add_scaled(&c, b, 2);
    NEAR(c.z, 15);
    v3_invert(&c);
    NEAR(c.y, -12);
    V3 n = v3_create(0, 0, 5);
    v3_normalize(&n);
    NEAR(n.z, 1);
    V3 zero = v3_zero();
    v3_normalize(&zero);                 // must not divide by zero
    NEAR(zero.x, 0);
    NEAR(v3_lerp(a, b, 0.5f).z, 4.5f);
}

static void test_free_fall(void) {
    Particle p = {0};
    p.position = v3_create(0, 10, 0);
    p.acceleration = v3_create(0, -10, 0);
    p.damping = 1;
    for (i32 i = 0; i < 60; i++) particle_integrate(&p, 1.0f / 60.0f);
    NEAR(p.velocity.y, -10);
    NEAR(p.position.y, 10 - 10.0f * 1830.0f / 3600.0f); // sum of k*dt*g*dt, k = 1..60 (velocity is updated first)
}

// the book's integrator gained energy each step, so a 10 cm hop with restitution 0.85 went on forever
static void test_small_bounce_settles(void) {
    Particle p = {0};
    p.position = v3_create(0, 0.6f, 0);
    p.acceleration = v3_create(0, -9.8f, 0);
    p.damping = 1;
    particle_set_mass(&p, 1);
    V3 lo = v3_create(-10, 0, -10), hi = v3_create(10, 10, 10);
    for (i32 i = 0; i < 60 * 10; i++) {
        particle_integrate(&p, 1.0f / 60.0f);
        particle_collide_box(&p, 0.5f, lo, hi, 0.85f, 0);
    }
    NEAR(p.position.y, 0.5f);
    NEAR(p.velocity.y, 0);
}

static void test_forces(void) {
    Particle p = {0};
    p.damping = 1;
    particle_set_mass(&p, 2);
    particle_add_force(&p, v3_create(10, 0, 0));
    particle_integrate(&p, 1);
    NEAR(p.velocity.x, 5);               // 10 N / 2 kg for 1 s
    particle_integrate(&p, 1);
    NEAR(p.velocity.x, 5);               // force was cleared, no more push

    Particle wall = {0};                 // inverse_mass 0 = infinite mass
    wall.damping = 1;
    particle_add_force(&wall, v3_create(1000, 0, 0));
    particle_integrate(&wall, 1);
    NEAR(wall.velocity.x, 0);
}

static void test_force_gravity(void) {
    ForceGen g = force_gravity(v3_create(0, -10, 0));
    Particle p = {0};
    p.damping = 1;
    particle_set_mass(&p, 2);
    force_update(&g, &p, 1);
    NEAR(p.force_accum.y, -20);          // mass * gravity
    particle_integrate(&p, 1);
    NEAR(p.velocity.y, -10);             // same acceleration for any mass

    Particle wall = {0};                 // infinite mass gets no force (get_mass would be REAL_MAX)
    force_update(&g, &wall, 1);
    NEAR(wall.force_accum.y, 0);
}

static void test_force_drag(void) {
    ForceGen d = force_drag(1, 0.5f);
    Particle p = {0};
    p.velocity = v3_create(0, 0, -4);
    force_update(&d, &p, 1);
    NEAR(p.force_accum.z, 12);           // against the motion: 1 * 4 + 0.5 * 16
    NEAR(p.force_accum.x, 0);

    Particle still = {0};                // at rest: zero force, not NaN
    force_update(&d, &still, 1);
    CHECK(still.force_accum.x == 0 && still.force_accum.y == 0 && still.force_accum.z == 0);

    // falling with gravity and k2 drag tops out at sqrt(m g / k2) = sqrt(1 * 10 / 0.1) = 10 m/s
    ForceGen g = force_gravity(v3_create(0, -10, 0));
    ForceGen air = force_drag(0, 0.1f);
    Particle ball = {0};
    ball.damping = 1;
    particle_set_mass(&ball, 1);
    for (i32 i = 0; i < 60 * 20; i++) {
        force_update(&g, &ball, 1.0f / 60.0f);
        force_update(&air, &ball, 1.0f / 60.0f);
        particle_integrate(&ball, 1.0f / 60.0f);
    }
    CHECK(near(ball.velocity.y, -10, 0.01f));
}

static void test_force_attract(void) {
    Particle a = {0}, b = {0};
    particle_set_mass(&a, 2);
    particle_set_mass(&b, 3);
    b.position = v3_create(2, 0, 0);
    ForceGen to_a = force_attract(&a, 1), to_b = force_attract(&b, 1);
    force_update(&to_b, &a, 1);
    force_update(&to_a, &b, 1);
    NEAR(a.force_accum.x, 1.5f);         // 1 * 2 * 3 / 2^2, toward b
    NEAR(b.force_accum.x, -1.5f);        // equal and opposite (Newton 3)
    NEAR(a.force_accum.y, 0);

    particle_clear_force_accum(&a);
    force_update(&to_a, &a, 1);          // no pull on itself
    CHECK(a.force_accum.x == 0);

    b.position = a.position;             // same spot: zero force, not NaN
    force_update(&to_b, &a, 1);
    CHECK(a.force_accum.x == 0 && a.force_accum.y == 0 && a.force_accum.z == 0);

    Particle wall = {0};                 // infinite mass: no pull either way
    wall.position = v3_create(1, 0, 0);
    ForceGen to_wall = force_attract(&wall, 1);
    force_update(&to_wall, &a, 1);
    force_update(&to_a, &wall, 1);
    CHECK(a.force_accum.x == 0 && wall.force_accum.x == 0);
}

static void test_force_explode(void) {
    ForceGen blast = force_explode(v3_create(1, 0, 0), 10);
    Particle light = {0}, heavy = {0}, middle = {0};
    particle_set_mass(&light, 1);
    particle_set_mass(&heavy, 100);
    light.position = v3_create(4, 0, 0);
    heavy.position = v3_create(1, -2, 0);
    middle.position = v3_create(1, 0, 0);
    force_update(&blast, &light, 1);
    force_update(&blast, &heavy, 1);
    force_update(&blast, &middle, 1);
    NEAR(light.force_accum.x, 10);       // straight away from the center
    NEAR(heavy.force_accum.y, -10);      // same force whatever the mass
    NEAR(heavy.force_accum.x, 0);
    CHECK(middle.force_accum.x == 0 && middle.force_accum.y == 0 && middle.force_accum.z == 0); // at the center, not NaN
}

static void test_force_registry(void) {
    ForceGen g = force_gravity(v3_create(0, -10, 0));
    ForceGen d = force_drag(1, 0);
    Particle a = {0}, b = {0};
    particle_set_mass(&a, 1);
    particle_set_mass(&b, 3);
    b.velocity = v3_create(2, 0, 0);

    ForceReg regs[3];
    ForceRegistry r = {regs, 0, 3};
    force_registry_add(&r, &a, &g);
    force_registry_add(&r, &b, &g);       // one generator shared by two particles
    force_registry_add(&r, &b, &d);
    force_registry_update(&r, 1);
    NEAR(a.force_accum.y, -10);
    NEAR(b.force_accum.y, -30);
    NEAR(b.force_accum.x, -2);           // forces on b add up (D'Alembert)

    force_registry_remove(&r, &a, &g);   // first one: the last pair moves into its place
    CHECK(r.count == 2);
    CHECK(r.regs[0].particle == &b && r.regs[0].gen == &d);
    force_registry_remove(&r, &a, &d);   // not registered: no effect
    CHECK(r.count == 2);

    particle_clear_force_accum(&a);
    particle_clear_force_accum(&b);
    force_registry_update(&r, 1);
    NEAR(a.force_accum.y, 0);            // a got nothing
    NEAR(b.force_accum.y, -30);
    NEAR(b.force_accum.x, -2);

    force_registry_clear(&r);
    CHECK(r.count == 0);
}

static void test_mass(void) {
    Particle p = {0};
    particle_set_mass(&p, 4);
    NEAR(p.inverse_mass, 0.25f);
    NEAR(particle_get_mass(&p), 4);
    CHECK(particle_has_finite_mass(&p));
    particle_set_inverse_mass(&p, 0);
    CHECK(!particle_has_finite_mass(&p));
    CHECK(particle_get_mass(&p) == REAL_MAX);
}

static void test_damping(void) {
    Particle p = {0};
    p.velocity = v3_create(10, 0, 0);
    p.damping = 0.5f;
    particle_integrate(&p, 1.0f / 60.0f);
    NEAR(p.velocity.x, 5);
}

static void test_box(void) {
    V3 lo = v3_create(0, 0, -1), hi = v3_create(16, 9, 1);

    Particle p = {0};
    p.position = v3_create(5, 0.1f, 0);     // below the floor for radius 0.5
    p.velocity = v3_create(2, -4, 0);
    particle_collide_box(&p, 0.5f, lo, hi, 0.5f, 0.1f);
    NEAR(p.position.y, 0.5f);
    NEAR(p.velocity.y, 2);               // bounced, half speed
    NEAR(p.velocity.x, 1.8f);            // lost 10% sideways

    Particle q = {0};
    q.position = v3_create(15.9f, 4, 0);
    q.velocity = v3_create(6, 0, 0);
    particle_collide_box(&q, 0.5f, lo, hi, 1, 0);
    NEAR(q.position.x, 15.5f);
    NEAR(q.velocity.x, -6);

    Particle slow = {0};                 // resting on the floor: gravity speed only, no bounce
    slow.position = v3_create(5, 0.4f, 0);
    slow.velocity = v3_create(0, -0.2f, 0);
    particle_collide_box(&slow, 0.5f, lo, hi, 0.9f, 0);
    NEAR(slow.velocity.y, 0);

    Particle r = {0};
    r.position = v3_create(8, 4, 0);
    r.velocity = v3_create(1, 1, 0);
    particle_collide_box(&r, 0.5f, lo, hi, 0.5f, 0.1f);
    NEAR(r.velocity.x, 1);               // inside the box, untouched

    Particle back = {0};                 // through the back wall (z), loses speed on both other axes
    back.position = v3_create(8, 4, -0.8f);
    back.velocity = v3_create(1, 2, -4);
    particle_collide_box(&back, 0.5f, lo, hi, 0.5f, 0.1f);
    NEAR(back.position.z, -0.5f);
    NEAR(back.velocity.z, 2);
    NEAR(back.velocity.x, 0.9f);
    NEAR(back.velocity.y, 1.8f);
}

static Particle make_ball(real x, real y, real vx, real vy, real mass) {
    Particle p = {0};
    p.position = v3_create(x, y, 0);
    p.velocity = v3_create(vx, vy, 0);
    p.damping = 1;
    if (mass > 0) particle_set_mass(&p, mass);
    return p;
}

static void test_spheres(void) {
    // head on, equal mass, elastic: they swap velocities
    Particle a = make_ball(0, 0, 4, 0, 1), b = make_ball(0.9f, 0, -4, 0, 1);
    CHECK(particle_collide_spheres(&a, 0.5f, &b, 0.5f, 1) == 1);
    NEAR(a.velocity.x, -4);
    NEAR(b.velocity.x, 4);
    NEAR(b.position.x - a.position.x, 1);        // pushed apart until just touching

    // no overlap: untouched
    Particle c = make_ball(0, 0, 1, 0, 1), d = make_ball(2, 0, -1, 0, 1);
    CHECK(particle_collide_spheres(&c, 0.5f, &d, 0.5f, 1) == 0);
    NEAR(c.velocity.x, 1);

    // overlapping but already moving apart: pushed out, velocity kept
    Particle e = make_ball(0, 0, -3, 0, 1), f = make_ball(0.8f, 0, 3, 0, 1);
    CHECK(particle_collide_spheres(&e, 0.5f, &f, 0.5f, 1) == 1);
    NEAR(e.velocity.x, -3);
    NEAR(f.position.x - e.position.x, 1);

    // different masses: momentum and energy are conserved when elastic
    Particle g = make_ball(0, 0, 5, 1, 3), h = make_ball(0.9f, 0.2f, -2, 0, 1);
    f32 px = 3 * g.velocity.x + 1 * h.velocity.x, py = 3 * g.velocity.y + 1 * h.velocity.y;
    f32 energy = 0.5f * 3 * v3_square_magnitude(g.velocity) + 0.5f * 1 * v3_square_magnitude(h.velocity);
    CHECK(particle_collide_spheres(&g, 0.5f, &h, 0.5f, 1) == 1);
    NEAR(3 * g.velocity.x + 1 * h.velocity.x, px);
    NEAR(3 * g.velocity.y + 1 * h.velocity.y, py);
    NEAR(0.5f * 3 * v3_square_magnitude(g.velocity) + 0.5f * 1 * v3_square_magnitude(h.velocity), energy);

    // inelastic: both leave with the same velocity along the normal
    Particle i = make_ball(0, 0, 4, 0, 1), j = make_ball(0.9f, 0, 0, 0, 1);
    particle_collide_spheres(&i, 0.5f, &j, 0.5f, 0);
    NEAR(i.velocity.x, 2);
    NEAR(j.velocity.x, 2);

    // infinite mass (a wall): it stays, the ball bounces off
    Particle wall = make_ball(0, 0, 0, 0, 0), ball = make_ball(0.9f, 0, -4, 0, 1);
    particle_collide_spheres(&wall, 0.5f, &ball, 0.5f, 1);
    NEAR(wall.position.x, 0);
    NEAR(wall.velocity.x, 0);
    NEAR(ball.velocity.x, 4);
    NEAR(ball.position.x, 1);

    // two walls: nothing to do
    Particle w1 = make_ball(0, 0, 0, 0, 0), w2 = make_ball(0.5f, 0, 0, 0, 0);
    CHECK(particle_collide_spheres(&w1, 0.5f, &w2, 0.5f, 1) == 0);

    // slow impact does not bounce
    Particle k = make_ball(0, 0, 0.2f, 0, 1), l = make_ball(0.9f, 0, 0, 0, 1);
    particle_collide_spheres(&k, 0.5f, &l, 0.5f, 1);
    NEAR(k.velocity.x, 0.1f);
    NEAR(l.velocity.x, 0.1f);

    // same position: must not produce NaN
    Particle m = make_ball(1, 1, 0, 0, 1), n = make_ball(1, 1, 0, 0, 1);
    CHECK(particle_collide_spheres(&m, 0.5f, &n, 0.5f, 1) == 1);
    CHECK(m.position.x == m.position.x && n.position.x == n.position.x);
    NEAR(v3_magnitude(v3_subtracted(n.position, m.position)), 1);
}

static void test_spheres_3d(void) {
    // diagonal hit in 3D, equal mass, elastic: they swap velocities along the line between them
    real s = 0.9f / real_sqrt(3);        // 0.9 apart along (1, 1, 1)
    Particle a = make_ball(0, 0, 0, 0, 1), b = make_ball(s, s, 0, 0, 1);
    a.velocity = v3_create(2, 2, 2);
    b.position.z = s;
    CHECK(particle_collide_spheres(&a, 0.5f, &b, 0.5f, 1) == 1);
    NEAR(a.velocity.z, 0);
    NEAR(b.velocity.x, 2);
    NEAR(b.velocity.z, 2);
    NEAR(v3_magnitude(v3_subtracted(b.position, a.position)), 1);
}

static void test_rand_unit(void) {
    rand_seed(7);
    V3 sum = v3_zero();
    for (i32 i = 0; i < 10000; i++) {
        V3 v = rand_unit_v3();
        CHECK(near(v3_magnitude(v), 1, 0.0001f));
        v3_add(&sum, v);
    }
    CHECK(v3_magnitude(sum) / 10000 < 0.03f); // no favored direction: they average out to about zero
}

static void test_rand(void) {
    rand_seed(42);
    u32 first = rand_u32();
    rand_seed(42);
    CHECK(rand_u32() == first);          // same seed, same numbers
    for (i32 i = 0; i < 10000; i++) {
        f32 v = rand_f32(-2, 3);
        CHECK(v >= -2 && v < 3);
    }
    rand_seed(0);
    CHECK(rand_u32() != 0);              // seed 0 must not get stuck
}

static void test_trig(void) {
    NEAR(f32_sin(0), 0);
    NEAR(f32_sin(F32_PI / 6), 0.5f);
    NEAR(f32_sin(F32_PI / 2), 1);
    NEAR(f32_sin(-F32_PI / 6), -0.5f);
    NEAR(f32_sin(F32_PI * 7 / 6), -0.5f);
    NEAR(f32_sin(F32_PI / 6 + 20 * F32_PI), 0.5f); // far from zero still folds back
    NEAR(f32_cos(0), 1);
    NEAR(f32_cos(F32_PI), -1);
    NEAR(f32_cos(F32_PI / 3), 0.5f);
    NEAR(f32_tan(F32_PI / 4), 1);
    f32 worst = 0;
    for (f32 x = -10; x < 10; x += 0.01f) {
        f32 s = f32_sin(x), c = f32_cos(x);
        f32 err = s * s + c * c - 1;
        if (err < 0) err = -err;
        if (err > worst) worst = err;
    }
    CHECK(worst < 0.00001f);                 // sin^2 + cos^2 = 1 everywhere
}

static void test_m4(void) {
    M4 a = m4_identity();
    a.m[12] = 5;                             // move x by 5
    M4 b = m4_identity();
    b.m[0] = 2;                              // scale x by 2
    V3 p = m4_transform_point(m4_multiplied(a, b), v3_create(1, 0, 0));
    NEAR(p.x, 7);                            // scaled first, then moved
    p = m4_transform_point(m4_multiplied(b, a), v3_create(1, 0, 0));
    NEAR(p.x, 12);                           // moved first, then scaled

    M4 view = m4_look_at(v3_create(0, 0, 5), v3_zero(), v3_create(0, 1, 0));
    p = m4_transform_point(view, v3_zero());
    NEAR(p.z, -5);                           // the target is 5 in front, down -z
    p = m4_transform_point(view, v3_create(1, 2, 0));
    NEAR(p.x, 1);                            // right stays right
    NEAR(p.y, 2);                            // up stays up

    M4 proj = m4_perspective(F32_PI / 2, 2, 1, 100);
    NEAR(m4_transform_point(proj, v3_create(0, 0, -1)).z, 0);   // near plane -> depth 0
    NEAR(m4_transform_point(proj, v3_create(0, 0, -100)).z, 1); // far plane -> depth 1
    NEAR(m4_transform_point(proj, v3_create(0, 3, -3)).y, 1);   // 45 degrees up is the top edge (fov 90)
    NEAR(m4_transform_point(proj, v3_create(6, 0, -3)).x, 1);   // aspect 2: twice as wide

    M4 ortho = m4_orthographic(-2, 6, 1, 3, 1, 11);
    p = m4_transform_point(ortho, v3_create(-2, 1, -1));
    NEAR(p.x, -1);                           // left, bottom, near corner
    NEAR(p.y, -1);
    NEAR(p.z, 0);
    p = m4_transform_point(ortho, v3_create(6, 3, -11));
    NEAR(p.x, 1);                            // right, top, far corner
    NEAR(p.y, 1);
    NEAR(p.z, 1);
    NEAR(m4_transform_point(ortho, v3_create(2, 2, -50)).x, 0); // no perspective: distance does not move x
}

static void test_str(void) {
    char line[32];
    line[0] = 0;
    str_append(line, sizeof(line), "n: ");
    str_append_i32(line, sizeof(line), -1205);
    CHECK(str_len(line) == 8);
    CHECK(line[3] == '-' && line[7] == '5');
    char tiny[4];
    tiny[0] = 0;
    str_append(tiny, sizeof(tiny), "abcdef");
    CHECK(str_len(tiny) == 3);           // truncated, still terminated
}

__attribute__((export_name("run_tests"))) i32 run_tests(void) {
    failures = 0;
    test_vectors();
    test_free_fall();
    test_small_bounce_settles();
    test_forces();
    test_force_gravity();
    test_force_drag();
    test_force_attract();
    test_force_explode();
    test_force_registry();
    test_mass();
    test_damping();
    test_box();
    test_spheres();
    test_spheres_3d();
    test_rand_unit();
    test_rand();
    test_trig();
    test_m4();
    test_str();
    return failures;
}
