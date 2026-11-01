#include "base/base.h"
#include "platform/platform.h"
#include "phys/phys.h"
#include "render/render.h"
#include "demo/demo.h"

// Rope Bridge (book 6.2, fig 6.2): a row of balls, each linked to its neighbours by springs and hung from a beam
// above by a cable (an anchored spring or bungee). The end balls are tied to the banks by anchored springs.
// Gravity is a force generator too. Space drops a heavy ball on it (our contacts), R resets.
// Every spring is drawn as a line: white at rest length, red stretched, blue compressed, grey for a slack bungee.

#define BRIDGE_N 16          // balls in the deck
#define BRIDGE_DROPS 20      // dropped balls kept, then the oldest is reused
#define BRIDGE_BODIES (BRIDGE_N + BRIDGE_DROPS)
#define BRIDGE_GAP 0.6f      // m, rest length of a link
#define BRIDGE_RADIUS 0.25f  // m, deck ball
#define BRIDGE_MASS 2.0f     // kg, deck ball
#define BRIDGE_DECK 4.0f     // m, deck height at rest length
#define BRIDGE_CABLE 3.0f    // m, cable rest length, so the beam is this far above the deck
#define BRIDGE_DROP_RADIUS 0.4f
#define BRIDGE_BOUNCE 0.3f
#define BRIDGE_FLOOR_FRICTION 0.05f // share of sideways speed lost per step on the floor
#define BRIDGE_SOLVER_PASSES 4
#define BRIDGE_MAX_CONTACTS (BRIDGE_BODIES * 16)
#define BRIDGE_FORCES (BRIDGE_BODIES + BRIDGE_N * 3) // gravity on all, two links and a cable per deck ball

enum { BRIDGE_CABLE_SPRING, BRIDGE_CABLE_BUNGEE };

// tuned live from the debug panel, all act right away
static f32 bridge_link_k = 500;   // N/m
static f32 bridge_cable_k = 100;  // N/m
static i32 bridge_cable_kind = BRIDGE_CABLE_SPRING;
static f32 bridge_damping = 0.995f; // deck balls, velocity kept per step
static f32 bridge_drop_mass = 10;   // kg, for the next drop

typedef struct {
    Particle particle;
    V3 previous_position; // from the step before, for blending
    real radius;
    real reach; // how far it can move this step, for the contact margin
} BridgeBody;

// deck balls first, then the drops
static BridgeBody bridge_bodies[BRIDGE_BODIES];
static i32 bridge_drops;     // drops in use
static i32 bridge_drop_next; // the slot the next drop goes into once all are in use

// one gravity for everything. each deck ball has a link toward the ball before and after it (an anchored spring
// to the bank at the ends) and a cable up to the beam
static ForceGen bridge_gravity;
static ForceGen bridge_to_prev[BRIDGE_N], bridge_to_next[BRIDGE_N], bridge_cables[BRIDGE_N];
static V3 bridge_bank_left, bridge_bank_right; // where the end links are tied
static V3 bridge_anchors[BRIDGE_N];             // cable tops on the beam
static ForceReg bridge_regs[BRIDGE_FORCES];
static ForceRegistry bridge_forces = {bridge_regs, 0, BRIDGE_FORCES};

static i32 bridge_stats_contacts; // per second, see bridge_stats
static i32 bridge_stats_full;     // steps per second where the contact list ran full

// this step's contacts and last step's (for the warm start), swapped every step
static ParticleContact bridge_contact_lists[2][BRIDGE_MAX_CONTACTS];
static i32 bridge_contact_counts[2], bridge_contacts_now;

// writes the slider values into the generators. the registry holds pointers to them, so nothing else changes
static void bridge_set_forces(void) {
    for (i32 i = 0; i < BRIDGE_N; i++) {
        bridge_to_prev[i] = i == 0 ? force_anchored_spring(bridge_bank_left, bridge_link_k, BRIDGE_GAP)
                                   : force_spring(&bridge_bodies[i - 1].particle, bridge_link_k, BRIDGE_GAP);
        bridge_to_next[i] = i == BRIDGE_N - 1 ? force_anchored_spring(bridge_bank_right, bridge_link_k, BRIDGE_GAP)
                                              : force_spring(&bridge_bodies[i + 1].particle, bridge_link_k, BRIDGE_GAP);
        bridge_cables[i] = bridge_cable_kind == BRIDGE_CABLE_BUNGEE
                               ? force_anchored_bungee(bridge_anchors[i], bridge_cable_k, BRIDGE_CABLE)
                               : force_anchored_spring(bridge_anchors[i], bridge_cable_k, BRIDGE_CABLE);
        bridge_bodies[i].particle.damping = bridge_damping;
    }
}

// the deck at rest length, centered on x = 0, no drops
static void bridge_reset(void) {
    real span = (BRIDGE_N + 1) * BRIDGE_GAP; // bank to bank
    bridge_bank_left = v3_create(-span / 2, BRIDGE_DECK, 0);
    bridge_bank_right = v3_create(span / 2, BRIDGE_DECK, 0);
    bridge_gravity = force_gravity(v3_create(0, -9.8f, 0));
    force_registry_clear(&bridge_forces);
    for (i32 i = 0; i < BRIDGE_N; i++) {
        BridgeBody *body = &bridge_bodies[i];
        Particle *p = &body->particle;
        p->position = v3_create(bridge_bank_left.x + (i + 1) * BRIDGE_GAP, BRIDGE_DECK, 0);
        p->velocity = v3_zero();
        p->acceleration = v3_zero();
        p->force_accum = v3_zero();
        particle_set_mass(p, BRIDGE_MASS);
        body->previous_position = p->position;
        body->radius = BRIDGE_RADIUS;
        bridge_anchors[i] = v3_create(p->position.x, BRIDGE_DECK + BRIDGE_CABLE, 0);
    }
    bridge_set_forces();
    for (i32 i = 0; i < BRIDGE_N; i++) {
        Particle *p = &bridge_bodies[i].particle;
        force_registry_add(&bridge_forces, p, &bridge_gravity);
        force_registry_add(&bridge_forces, p, &bridge_to_prev[i]);
        force_registry_add(&bridge_forces, p, &bridge_to_next[i]);
        force_registry_add(&bridge_forces, p, &bridge_cables[i]);
    }
    bridge_drops = 0;
    bridge_drop_next = 0;
    bridge_contact_counts[0] = bridge_contact_counts[1] = 0; // new bodies, nothing to warm start from
}

// a heavy ball above a random spot on the deck
static void bridge_drop(void) {
    i32 slot;
    if (bridge_drops < BRIDGE_DROPS) {
        slot = bridge_drops++;
        force_registry_add(&bridge_forces, &bridge_bodies[BRIDGE_N + slot].particle, &bridge_gravity);
    } else {
        slot = bridge_drop_next;
        bridge_drop_next = (bridge_drop_next + 1) % BRIDGE_DROPS;
    }
    BridgeBody *body = &bridge_bodies[BRIDGE_N + slot];
    Particle *p = &body->particle;
    real reach = bridge_bank_right.x - BRIDGE_GAP;
    p->position = v3_create(rand_f32(-reach, reach), BRIDGE_DECK + BRIDGE_CABLE - 0.5f, 0);
    p->velocity = v3_zero();
    p->acceleration = v3_zero();
    p->force_accum = v3_zero();
    p->damping = 1;
    particle_set_mass(p, bridge_drop_mass);
    body->previous_position = p->position;
    body->radius = BRIDGE_DROP_RADIUS;
}

static void bridge_init(void) {
    plat_ui_slider_log("link k N/m", 10, 20000, 1, &bridge_link_k);
    plat_ui_slider_log("cable k N/m", 10, 20000, 1, &bridge_cable_k);
    plat_ui_choice("cables", "Spring|Bungee", &bridge_cable_kind);
    plat_ui_slider("damping", 0.9f, 1, 0.001f, &bridge_damping);
    plat_ui_slider_log("drop kg", 1, 200, 0.5f, &bridge_drop_mass);

    rand_seed(12345);
    bridge_reset();
}

static void bridge_update(void) {
    if (plat_key_pressed(KEY_R)) bridge_reset();
    if (plat_key_pressed(KEY_SPACE)) bridge_drop();
    bridge_set_forces();
}

FORCE_INLINE void bridge_step(real dt) {
    force_registry_update(&bridge_forces, dt);
    i32 alive = BRIDGE_N + bridge_drops;
    for (i32 i = 0; i < alive; i++) {
        BridgeBody *body = &bridge_bodies[i];
        body->previous_position = body->particle.position;
        particle_integrate_velocity(&body->particle, dt);
        body->reach = v3_magnitude(body->particle.velocity) * dt;
    }

    // every pair that could touch this step, and the floor (y = 0). found in (i, j) order, so the list is sorted
    // for the warm start. the floor's id is BRIDGE_BODIES, after every body
    i32 before = bridge_contacts_now;
    bridge_contacts_now ^= 1;
    ParticleContact *list = bridge_contact_lists[bridge_contacts_now];
    i32 count = 0;
    for (i32 i = 0; i < alive && count < BRIDGE_MAX_CONTACTS - 1; i++) {
        BridgeBody *a = &bridge_bodies[i];
        for (i32 j = i + 1; j < alive && count < BRIDGE_MAX_CONTACTS - 1; j++) {
            BridgeBody *b = &bridge_bodies[j];
            if (particle_contact_find_spheres(&list[count], &a->particle, a->radius, &b->particle, b->radius,
                                              BRIDGE_BOUNCE, dt, a->reach + b->reach)) {
                list[count].id_a = i;
                list[count].id_b = j;
                count++;
            }
        }
        if (particle_contact_find_plane(&list[count], &a->particle, a->radius, v3_create(0, 1, 0), 0,
                                        BRIDGE_BOUNCE, BRIDGE_FLOOR_FRICTION, dt, a->reach)) {
            list[count].id_a = i;
            list[count].id_b = BRIDGE_BODIES;
            count++;
        }
    }
    if (count >= BRIDGE_MAX_CONTACTS - 1) bridge_stats_full++;
    bridge_contact_counts[bridge_contacts_now] = count;
    bridge_stats_contacts += count;

    particle_contacts_warm(list, count, bridge_contact_lists[before], bridge_contact_counts[before]);
    particle_contacts_solve(list, count, BRIDGE_SOLVER_PASSES);
    for (i32 i = 0; i < alive; i++) particle_integrate_position(&bridge_bodies[i].particle, dt);
    particle_contacts_bounce(list, count, BRIDGE_SOLVER_PASSES);
}

// white at rest length, red stretched, blue compressed (full color at 25%). a slack bungee is grey: it does nothing
static void bridge_draw_spring(V3 from, V3 to, real rest, i32 pull_only) {
    real length = v3_magnitude(v3_subtracted(to, from));
    real strain = (length - rest) / rest;
    if (pull_only && strain <= 0) {
        render_draw_line(from, to, 0.4f, 0.4f, 0.4f);
        return;
    }
    real t = (strain < 0 ? -strain : strain) * 4;
    if (t > 1) t = 1;
    if (strain > 0) render_draw_line(from, to, 1, 1 - t, 1 - t);
    else render_draw_line(from, to, 1 - t, 1 - t, 1);
}

FORCE_INLINE void bridge_draw(f32 alpha, f32 seconds, f32 hud_top) {
    (void)seconds;
    V3 at[BRIDGE_BODIES];
    i32 alive = BRIDGE_N + bridge_drops;
    for (i32 i = 0; i < alive; i++) at[i] = v3_lerp(bridge_bodies[i].previous_position, bridge_bodies[i].particle.position, alpha);

    for (i32 i = 0; i < BRIDGE_N; i++) render_draw_sphere(at[i], BRIDGE_RADIUS, 0.75f, 0.55f, 0.35f);
    for (i32 i = BRIDGE_N; i < alive; i++) render_draw_sphere(at[i], BRIDGE_DROP_RADIUS, 0.45f, 0.6f, 0.9f);

    // springs: links along the deck (bank to bank), cables up to the beam
    bridge_draw_spring(bridge_bank_left, at[0], BRIDGE_GAP, 0);
    for (i32 i = 0; i + 1 < BRIDGE_N; i++) bridge_draw_spring(at[i], at[i + 1], BRIDGE_GAP, 0);
    bridge_draw_spring(at[BRIDGE_N - 1], bridge_bank_right, BRIDGE_GAP, 0);
    i32 bungee = bridge_cable_kind == BRIDGE_CABLE_BUNGEE;
    for (i32 i = 0; i < BRIDGE_N; i++) bridge_draw_spring(bridge_anchors[i], at[i], BRIDGE_CABLE, bungee);

    // the frame: beam, posts on the banks, banks, floor
    f32 frame = 0.5f;
    V3 beam_left = v3_create(bridge_bank_left.x, BRIDGE_DECK + BRIDGE_CABLE, 0);
    V3 beam_right = v3_create(bridge_bank_right.x, BRIDGE_DECK + BRIDGE_CABLE, 0);
    render_draw_line(beam_left, beam_right, frame, frame, frame);
    render_draw_line(bridge_bank_left, beam_left, frame, frame, frame);
    render_draw_line(bridge_bank_right, beam_right, frame, frame, frame);
    V3 bank_half = v3_create(1.5f, BRIDGE_DECK / 2, 1.5f);
    render_draw_box(v3_create(bridge_bank_left.x - bank_half.x, BRIDGE_DECK / 2, 0), bank_half, 0.35f, 0.33f, 0.30f);
    render_draw_box(v3_create(bridge_bank_right.x + bank_half.x, BRIDGE_DECK / 2, 0), bank_half, 0.35f, 0.33f, 0.30f);
    render_draw_box(v3_create(0, -0.05f, 0), v3_create(12, 0.05f, 5), 0.20f, 0.24f, 0.34f); // floor: a thin slab just under y = 0
    render_set_shadow_area(v3_create(-12, -0.1f, -5), v3_create(12, BRIDGE_DECK + BRIDGE_CABLE, 5));

    char line[64];
    line[0] = 0;
    str_append(line, sizeof(line), "drops: ");
    str_append_i32(line, sizeof(line), bridge_drops);
    render_draw_text(line, 16, hud_top, 2, 0.8f, 0.8f, 0.8f);
}

static void bridge_stats(char *line, i32 cap) {
    demo_append_stat(line, cap, "contacts/s", bridge_stats_contacts);
    demo_append_stat(line, cap, "drops", bridge_drops);
    demo_append_stat(line, cap, "contacts full", bridge_stats_full);
    bridge_stats_contacts = 0;
    bridge_stats_full = 0;
}
