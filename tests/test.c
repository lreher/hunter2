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

static void test_force_springs(void) {
    Particle a = {0}, b = {0};
    particle_set_mass(&a, 1);
    particle_set_mass(&b, 1);
    b.position = v3_create(3, 0, 0);
    ForceGen to_b = force_spring(&b, 2, 1), to_a = force_spring(&a, 2, 1);
    force_update(&to_b, &a, 1);
    force_update(&to_a, &b, 1);
    NEAR(a.force_accum.x, 4);            // stretched 2 m, k 2: pulls a toward b
    NEAR(b.force_accum.x, -4);           // and b toward a
    NEAR(a.force_accum.y, 0);

    particle_clear_force_accum(&a);
    b.position = v3_create(0.5f, 0, 0);
    force_update(&to_b, &a, 1);
    NEAR(a.force_accum.x, -1);           // compressed 0.5 m: pushes a away from b (the book's abs pulled here)

    particle_clear_force_accum(&a);
    b.position = a.position;
    force_update(&to_b, &a, 1);
    CHECK(a.force_accum.x == 0 && a.force_accum.y == 0 && a.force_accum.z == 0); // one spot: no NaN

    ForceGen hang = force_anchored_spring(v3_create(0, 5, 0), 10, 2);
    particle_clear_force_accum(&a);
    force_update(&hang, &a, 1);
    NEAR(a.force_accum.y, 30);           // stretched 3 m toward the anchor
    hang.anchored_spring.anchor = v3_create(0, 1, 0); // the game moves the anchor
    particle_clear_force_accum(&a);
    force_update(&hang, &a, 1);
    NEAR(a.force_accum.y, -10);          // compressed 1 m: pushes away

    ForceGen cord = force_bungee(&b, 2, 1);
    b.position = v3_create(0.5f, 0, 0);
    particle_clear_force_accum(&a);
    force_update(&cord, &a, 1);
    CHECK(a.force_accum.x == 0);         // slack: nothing
    b.position = v3_create(3, 0, 0);
    force_update(&cord, &a, 1);
    NEAR(a.force_accum.x, 4);            // stretched: a normal spring

    ForceGen jump = force_anchored_bungee(v3_create(0, 5, 0), 10, 2);
    a.position = v3_create(0, 4, 0);
    particle_clear_force_accum(&a);
    force_update(&jump, &a, 1);
    CHECK(a.force_accum.y == 0);         // 1 m from the anchor, rest 2: slack
    a.position = v3_create(0, 0, 0);
    force_update(&jump, &a, 1);
    NEAR(a.force_accum.y, 30);
}

// 1 kg on a spring with k = 4 pi^2 swings once per second. Semi-implicit Euler at 1/60 neither grows nor
// loses the swing: after 10 periods it is back at the start, and it never went past it
static void test_spring_oscillates(void) {
    Particle p = {0};
    particle_set_mass(&p, 1);
    p.damping = 1;
    p.position = v3_create(1, 0, 0);
    ForceGen s = force_anchored_spring(v3_create(0, 0, 0), 4 * F32_PI * F32_PI, 0);
    real widest = 0;
    for (i32 i = 0; i < 600; i++) {
        force_update(&s, &p, 1.0f / 60.0f);
        particle_integrate(&p, 1.0f / 60.0f);
        real x = p.position.x < 0 ? -p.position.x : p.position.x;
        if (x > widest) widest = x;
    }
    CHECK(near(p.position.x, 1, 0.01f));
    CHECK(widest < 1.01f);
}

static void test_force_buoyancy(void) {
    // water surface at y 2, depth 0.5, 0.01 m^3 in water 1000 at g 10: 100 N when all under
    ForceGen b = force_buoyancy(2, 0.5f, 0.01f, 1000, 10, 4);
    Particle p = {0};
    particle_set_mass(&p, 1);
    p.position = v3_create(0, 2.5f, 0);
    force_update(&b, &p, 1);
    CHECK(p.force_accum.y == 0);         // just out
    p.position.y = 2;
    force_update(&b, &p, 1);
    NEAR(p.force_accum.y, 50);           // center at the surface: half under
    particle_clear_force_accum(&p);
    p.position.y = 1.75f;
    force_update(&b, &p, 1);
    NEAR(p.force_accum.y, 75);           // straight in between
    particle_clear_force_accum(&p);
    p.position.y = -5;
    force_update(&b, &p, 1);
    NEAR(p.force_accum.y, 100);          // deep: no more than all under

    particle_clear_force_accum(&p);
    p.velocity = v3_create(1, -2, 0);
    force_update(&b, &p, 1);
    NEAR(p.force_accum.x, -4);           // drag against the motion, full under water
    NEAR(p.force_accum.y, 108);
    particle_clear_force_accum(&p);
    p.position.y = 2;
    force_update(&b, &p, 1);
    NEAR(p.force_accum.x, -2);           // half under: half the drag
    particle_clear_force_accum(&p);
    p.position.y = 3;
    force_update(&b, &p, 1);
    CHECK(p.force_accum.x == 0 && p.force_accum.y == 0); // in the air: nothing
}

// a body half as dense as the water floats half under, with its center on the surface. drag stops the bobbing
static void test_buoyancy_floats(void) {
    Particle p = {0};
    particle_set_mass(&p, 5);            // 0.01 m^3 at 500 kg/m^3
    p.damping = 1;
    p.acceleration = v3_create(0, -10, 0);
    p.position = v3_create(0, 4, 0);
    ForceGen b = force_buoyancy(2, 0.5f, 0.01f, 1000, 10, 20);
    for (i32 i = 0; i < 600; i++) {
        force_update(&b, &p, 1.0f / 60.0f);
        particle_integrate(&p, 1.0f / 60.0f);
    }
    CHECK(near(p.position.y, 2, 0.01f));
    CHECK(near(p.velocity.y, 0, 0.01f));
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

static Particle make_ball(real x, real y, real vx, real vy, real mass) {
    Particle p = {0};
    p.position = v3_create(x, y, 0);
    p.velocity = v3_create(vx, vy, 0);
    p.damping = 1;
    if (mass > 0) particle_set_mass(&p, mass);
    return p;
}

// one step in the contact order (phys.h, Contacts), like a demo runs it: spheres of radius 0.5, the floor y = 0 if floor
#define TEST_DT (1.0f / 60.0f)
static ParticleContact test_contact_lists[2][64];
static i32 test_contact_counts[2], test_contacts_now;

static void test_contacts_reset(void) {
    test_contact_counts[0] = test_contact_counts[1] = 0;
}

static void test_step(Particle *ps, i32 n, ForceRegistry *forces, i32 floor, real restitution) {
    if (forces) force_registry_update(forces, TEST_DT);
    real speeds[8];
    for (i32 i = 0; i < n; i++) {
        particle_integrate_velocity(&ps[i], TEST_DT);
        speeds[i] = v3_magnitude(ps[i].velocity);
    }
    i32 before = test_contacts_now;
    test_contacts_now ^= 1;
    ParticleContact *list = test_contact_lists[test_contacts_now];
    i32 count = 0;
    for (i32 i = 0; i < n; i++) {
        for (i32 j = i + 1; j < n; j++) {
            if (particle_contact_find_spheres(&list[count], &ps[i], 0.5f, &ps[j], 0.5f, restitution, TEST_DT,
                                              (speeds[i] + speeds[j]) * TEST_DT)) {
                list[count].id_a = i;
                list[count].id_b = j;
                count++;
            }
        }
        if (floor && particle_contact_find_plane(&list[count], &ps[i], 0.5f, v3_create(0, 1, 0), 0, restitution, 0,
                                                 TEST_DT, speeds[i] * TEST_DT)) {
            list[count].id_a = i;
            list[count].id_b = n; // after the pairs of i, so the list stays sorted
            count++;
        }
    }
    particle_contacts_warm(list, count, test_contact_lists[before], test_contact_counts[before]);
    test_contact_counts[test_contacts_now] = count;
    particle_contacts_solve(list, count, 4);
    for (i32 i = 0; i < n; i++) particle_integrate_position(&ps[i], TEST_DT);
    particle_contacts_bounce(list, count, 4);
}

// find, solve, move, bounce for one pair, by hand
static void test_pair_step(Particle *a, Particle *b, real restitution, real margin) {
    ParticleContact c;
    if (!particle_contact_find_spheres(&c, a, 0.5f, b, 0.5f, restitution, TEST_DT, margin)) return;
    particle_contacts_solve(&c, 1, 1);
    particle_integrate_position(a, TEST_DT);
    particle_integrate_position(b, TEST_DT);
    particle_contacts_bounce(&c, 1, 1);
}

static void test_contact_spheres(void) {
    ParticleContact c;

    // head on, 5 cm apart, elastic. the solver lets them close exactly the gap, then they bounce: velocities swap
    Particle a = make_ball(0, 0, 4, 0, 1), b = make_ball(1.05f, 0, -4, 0, 1);
    CHECK(particle_contact_find_spheres(&c, &a, 0.5f, &b, 0.5f, 1, TEST_DT, 8 * TEST_DT) == 1);
    NEAR(c.target, -3);                  // may close 0.05 m in 1/60 s
    NEAR(c.bounce, 8);
    particle_contacts_solve(&c, 1, 1);
    NEAR(a.velocity.x, 1.5f);
    NEAR(b.velocity.x, -1.5f);
    particle_integrate_position(&a, TEST_DT);
    particle_integrate_position(&b, TEST_DT);
    NEAR(b.position.x - a.position.x, 1); // just touching, not inside each other
    particle_contacts_bounce(&c, 1, 1);
    NEAR(a.velocity.x, -4);
    NEAR(b.velocity.x, 4);

    // further apart than the margin: no contact
    Particle d = make_ball(0, 0, 1, 0, 1), e = make_ball(1.5f, 0, -1, 0, 1);
    CHECK(particle_contact_find_spheres(&c, &d, 0.5f, &e, 0.5f, 1, TEST_DT, 2 * TEST_DT) == 0);

    // inside the margin but too slow to meet this step: the solver leaves them alone
    Particle f = make_ball(0, 0, 2, 0, 1), g = make_ball(1.1f, 0, -2, 0, 1);
    CHECK(particle_contact_find_spheres(&c, &f, 0.5f, &g, 0.5f, 1, TEST_DT, 0.2f) == 1);
    NEAR(c.bounce, 0);
    particle_contacts_solve(&c, 1, 4);
    NEAR(c.impulse, 0);
    NEAR(f.velocity.x, 2);

    // different masses, off center, elastic: momentum and energy conserved
    Particle h = make_ball(0, 0, 5, 1, 3), k = make_ball(0.9f, 0.45f, -2, 0, 1);
    h.velocity.z = 0.5f;
    k.position.z = 0.1f;
    V3 momentum = v3_added(v3_multiplied(h.velocity, 3), k.velocity);
    f32 energy = 0.5f * 3 * v3_square_magnitude(h.velocity) + 0.5f * v3_square_magnitude(k.velocity);
    test_pair_step(&h, &k, 1, 8 * TEST_DT);
    V3 after = v3_added(v3_multiplied(h.velocity, 3), k.velocity);
    NEAR(after.x, momentum.x);
    NEAR(after.y, momentum.y);
    NEAR(after.z, momentum.z);
    NEAR(0.5f * 3 * v3_square_magnitude(h.velocity) + 0.5f * v3_square_magnitude(k.velocity), energy); // a grazing hit, fast
    CHECK(k.velocity.x > 0);             // it was hit

    // inelastic: after they meet, both move on together
    Particle ps[2] = {make_ball(0, 0, 4, 0, 1), make_ball(1.01f, 0, 0, 0, 1)};
    test_contacts_reset();
    for (i32 i = 0; i < 3; i++) test_step(ps, 2, 0, 0, 0);
    NEAR(ps[0].velocity.x, 2);
    NEAR(ps[1].velocity.x, 2);

    // slow impact (under PHYS_REST_SPEED) does not bounce, even when elastic
    Particle slow[2] = {make_ball(0, 0, 0.2f, 0, 1), make_ball(1.001f, 0, 0, 0, 1)};
    test_contacts_reset();
    for (i32 i = 0; i < 3; i++) test_step(slow, 2, 0, 0, 1);
    NEAR(slow[0].velocity.x, 0.1f);
    NEAR(slow[1].velocity.x, 0.1f);

    // infinite mass: it stays, the ball bounces off it
    Particle wall = make_ball(0, 0, 0, 0, 0), ball = make_ball(1.02f, 0, -4, 0, 1);
    test_pair_step(&wall, &ball, 1, 4 * TEST_DT);
    NEAR(wall.position.x, 0);
    NEAR(wall.velocity.x, 0);
    NEAR(ball.position.x, 1);
    NEAR(ball.velocity.x, 4);

    // two immovable things: nothing to do
    Particle w1 = make_ball(0, 0, 0, 0, 0), w2 = make_ball(0.5f, 0, 0, 0, 0);
    CHECK(particle_contact_find_spheres(&c, &w1, 0.5f, &w2, 0.5f, 1, TEST_DT, 0) == 0);

    // an overlap opens at PHYS_OVERLAP_FIX of its depth past the slop per step, by velocity
    Particle l = make_ball(0, 0, 0, 0, 1), m = make_ball(0.9f, 0, 0, 0, 1);
    CHECK(particle_contact_find_spheres(&c, &l, 0.5f, &m, 0.5f, 1, TEST_DT, 0) == 1);
    particle_contacts_solve(&c, 1, 1);
    NEAR(m.velocity.x - l.velocity.x, (0.1f - PHYS_OVERLAP_SLOP) * PHYS_OVERLAP_FIX / TEST_DT);

    // a small overlap is left alone
    Particle sa = make_ball(0, 0, 0, 0, 1), sb = make_ball(1 - PHYS_OVERLAP_SLOP / 2, 0, 0, 0, 1);
    CHECK(particle_contact_find_spheres(&c, &sa, 0.5f, &sb, 0.5f, 1, TEST_DT, 0) == 1);
    particle_contacts_solve(&c, 1, 1);
    NEAR(sb.velocity.x - sa.velocity.x, 0);

    // the fix is not carried to the next step: once the overlap is gone it would push too hard (piles shook)
    Particle two[2] = {make_ball(0, 0, 0, 0, 1), make_ball(0.9f, 0, 0, 0, 1)};
    test_contacts_reset();
    test_step(two, 2, 0, 0, 0);
    ParticleContact *last = &test_contact_lists[test_contacts_now][0];
    NEAR(last->impulse, 0);              // nothing to carry: no steady push between two free balls
    CHECK(last->fix_impulse > 0);

    // same position: must not produce NaN
    Particle n = make_ball(1, 1, 0, 0, 1), o = make_ball(1, 1, 0, 0, 1);
    CHECK(particle_contact_find_spheres(&c, &n, 0.5f, &o, 0.5f, 1, TEST_DT, 0) == 1);
    particle_contacts_solve(&c, 1, 1);
    CHECK(n.velocity.x == n.velocity.x && o.velocity.x == o.velocity.x);
    CHECK(o.velocity.x > n.velocity.x);
}

static void test_contact_plane(void) {
    ParticleContact c;
    V3 up = v3_create(0, 1, 0);

    // falling onto the floor from 5 cm: lands on it, bounces at half speed, loses 10% sideways
    Particle p = make_ball(3, 0.55f, 2, -6, 1);
    CHECK(particle_contact_find_plane(&c, &p, 0.5f, up, 0, 0.5f, 0.1f, TEST_DT, 6 * TEST_DT) == 1);
    NEAR(p.velocity.x, 1.8f);
    particle_contacts_solve(&c, 1, 1);
    particle_integrate_position(&p, TEST_DT);
    NEAR(p.position.y, 0.5f);
    particle_contacts_bounce(&c, 1, 1);
    NEAR(p.velocity.y, 3);

    // high above: no contact
    Particle q = make_ball(3, 2, 0, -1, 1);
    CHECK(particle_contact_find_plane(&c, &q, 0.5f, up, 0, 0.5f, 0, TEST_DT, TEST_DT) == 0);

    // a wall at x = 16 (normal pointing back into the box)
    Particle r = make_ball(15.45f, 4, 6, 0, 1);
    CHECK(particle_contact_find_plane(&c, &r, 0.5f, v3_create(-1, 0, 0), -16, 1, 0, TEST_DT, 6 * TEST_DT) == 1);
    particle_contacts_solve(&c, 1, 1);
    particle_integrate_position(&r, TEST_DT);
    particle_contacts_bounce(&c, 1, 1);
    NEAR(r.position.x, 15.5f);
    NEAR(r.velocity.x, -6);
}

// the book's integrator gained energy each step, so a 10 cm hop with restitution 0.85 went on forever
static void test_small_bounce_settles(void) {
    Particle p = make_ball(0, 0.6f, 0, 0, 1);
    p.acceleration = v3_create(0, -9.8f, 0);
    test_contacts_reset();
    for (i32 i = 0; i < 60 * 10; i++) test_step(&p, 1, 0, 1, 0.85f);
    NEAR(p.position.y, 0.5f);
    NEAR(p.velocity.y, 0);
}

static void test_contacts_warm(void) {
    Particle ps[6];
    for (i32 i = 0; i < 6; i++) ps[i] = make_ball((real)i, 0, 0, 0, 1);
    ParticleContact before[3] = {{.id_a = 0, .id_b = 1, .impulse = 2}, {.id_a = 0, .id_b = 3, .impulse = 5},
                                 {.id_a = 2, .id_b = 3, .impulse = 7}};
    ParticleContact now[4];
    i32 ids[4][2] = {{0, 1}, {0, 2}, {2, 3}, {4, 5}};
    for (i32 i = 0; i < 4; i++) {
        now[i] = (ParticleContact){.a = &ps[ids[i][0]], .b = &ps[ids[i][1]], .id_a = ids[i][0], .id_b = ids[i][1],
                                   .normal = v3_create(1, 0, 0), .total_inverse_mass = 2};
    }
    particle_contacts_warm(now, 4, before, 3);
    NEAR(now[0].impulse, 2);
    NEAR(now[1].impulse, 0);             // new pair
    NEAR(now[2].impulse, 7);
    NEAR(now[3].impulse, 0);
    NEAR(ps[1].velocity.x, 2);           // warm impulses are applied
    NEAR(ps[2].velocity.x, -7);
    NEAR(ps[3].velocity.x, 7);
}

// the bug that started the contacts: a turning cluster held by its own gravity. contacts must not change its
// angular momentum (the old push on positions made it spin faster and faster)
static V3 test_angular_momentum(Particle *ps, i32 n) {
    V3 center = v3_zero(), momentum = v3_zero();
    real mass = 0;
    for (i32 i = 0; i < n; i++) {
        real m = particle_get_mass(&ps[i]);
        v3_add_scaled(&center, ps[i].position, m);
        v3_add_scaled(&momentum, ps[i].velocity, m);
        mass += m;
    }
    v3_multiply(&center, 1 / mass);
    v3_multiply(&momentum, 1 / mass);
    V3 l = v3_zero();
    for (i32 i = 0; i < n; i++) {
        V3 r = v3_subtracted(ps[i].position, center), v = v3_subtracted(ps[i].velocity, momentum);
        v3_add_scaled(&l, v3_vector_product(r, v), particle_get_mass(&ps[i]));
    }
    return l;
}

static void test_contacts_spin(void) {
    // four touching balls (a tetrahedron, edge 1) turning around y, pulling on each other
    Particle ps[4];
    real h = 0.5f * real_sqrt(2.0f) / 2;  // corners (+-h, +-h, +-h) with an even number of minus signs: edge 1
    V3 corners[4] = {v3_create(h, h, h), v3_create(h, -h, -h), v3_create(-h, h, -h), v3_create(-h, -h, h)};
    for (i32 i = 0; i < 4; i++) {
        ps[i] = make_ball(0, 0, 0, 0, 1 + i); // different masses
        ps[i].position = corners[i];
        ps[i].velocity = v3_create(0.5f * corners[i].z, 0, -0.5f * corners[i].x);
    }
    ForceGen pulls[4];
    ForceReg regs[12];
    ForceRegistry forces = {regs, 0, 12};
    for (i32 j = 0; j < 4; j++) pulls[j] = force_attract(&ps[j], 1);
    for (i32 i = 0; i < 4; i++) for (i32 j = 0; j < 4; j++) if (i != j) force_registry_add(&forces, &ps[i], &pulls[j]);

    V3 start = test_angular_momentum(ps, 4);
    test_contacts_reset();
    real worst = 0;
    for (i32 step = 0; step < 60 * 20; step++) {
        test_step(ps, 4, &forces, 0, 0.3f);
        for (i32 i = 0; i < 4; i++) for (i32 j = i + 1; j < 4; j++) {
            real overlap = 1 - v3_magnitude(v3_subtracted(ps[j].position, ps[i].position));
            if (overlap > worst) worst = overlap;
        }
    }
    V3 end = test_angular_momentum(ps, 4);
    CHECK(near(end.x, start.x, 0.0005f) && near(end.y, start.y, 0.0005f) && near(end.z, start.z, 0.0005f));
    CHECK(worst < 0.002f);               // they stay touching, not sinking in
}

// a column of three on the floor stands still
static void test_contacts_pile(void) {
    Particle ps[3] = {make_ball(0, 0.5f, 0, 0, 1), make_ball(0, 1.5f, 0, 0, 2), make_ball(0, 2.5f, 0, 0, 3)};
    for (i32 i = 0; i < 3; i++) ps[i].acceleration = v3_create(0, -9.8f, 0);
    test_contacts_reset();
    for (i32 step = 0; step < 60 * 10; step++) test_step(ps, 3, 0, 1, 0.5f);
    for (i32 i = 0; i < 3; i++) {
        CHECK(near(ps[i].position.y, 0.5f + i, (i + 1) * PHYS_OVERLAP_SLOP)); // each contact below may keep the slop
        CHECK(near(ps[i].velocity.y, 0, 0.01f));
    }
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
    test_force_springs();
    test_spring_oscillates();
    test_force_buoyancy();
    test_buoyancy_floats();
    test_force_registry();
    test_mass();
    test_damping();
    test_contact_spheres();
    test_contact_plane();
    test_contacts_warm();
    test_contacts_spin();
    test_contacts_pile();
    test_rand_unit();
    test_rand();
    test_trig();
    test_m4();
    test_str();
    return failures;
}
