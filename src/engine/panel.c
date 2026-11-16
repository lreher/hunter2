#include "engine/engine.h"
#include "platform/platform.h"

// The parts in the panel (focus/engine.md, panel). A placing places again as soon as its numbers change. The tree: "objects", each object with its settings in blocks, then
// its actuators and emitters (each opens to show what it shares and a split, emitters also a type pick), then the
// actuator types and emitter types (each picks what it emits: sparks or an copies_only object) with their sliders,
// copy, delete (only when nothing uses one), rename (double click) and "+ new".
// A click writes one number into engine->panel_action (PANEL_ACTION: what, which part, an option). engine_update_panel
// does it on the next frame and the game builds the panel again. Slider save keys are the fold name + slider name
// ("engine/force N"). The parts list (which types there are, which type each actuator and emitter uses) is a text
// saved with the panel as "parts", by name:
//   actuator push engine
//   emitter main
//   emitter machine gun=bullet       (sends out copies of the object "bullet")
//   actuator use right=side thrust
//   emitter use reverse left=side
// Load puts a saved list back, engine_update_panel applies it. max particles is code only (render_particle_tuning,
// user). Emitters on their own (no object) are not shown yet.

enum {
    PANEL_NONE,
    PANEL_NEW_ACTUATOR_TYPE,    // option: ACTUATOR_PUSH or ACTUATOR_TURN
    PANEL_COPY_ACTUATOR_TYPE,   // index: the type
    PANEL_DELETE_ACTUATOR_TYPE, // index: the type, only when nothing uses it
    PANEL_RENAME_ACTUATOR_TYPE, // index: the type, the new name is in panel_name
    PANEL_SPLIT_ACTUATOR,       // index: the actuator, it gets its own copy of its type
    PANEL_NEW_EMITTER_TYPE,
    PANEL_COPY_EMITTER_TYPE,
    PANEL_DELETE_EMITTER_TYPE,
    PANEL_RENAME_EMITTER_TYPE,
    PANEL_SPLIT_EMITTER,
    PANEL_PICK_EMITTER_TYPE,    // index: the emitter, option: the type
    PANEL_PICK_EMITS,           // index: the emitter type, option: 0 sparks, 1 sparks once, k = the (k - 1)-th copies_only object
    PANEL_PICK_HIT_BURST,       // index: the object, option: 0 nothing, k = emitter type k - 1 bursts where its copy hit
    PANEL_PLACE_AGAIN,          // index: the placing, its copies are put down fresh
};
#define PANEL_ACTION(what, index) ((what) << 16 | (index) << 8) // + the option, in the low 8 bits

static i32 panel_same(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static i32 panel_starts_with(const char *text, const char *start) {
    while (*start) if (*text++ != *start++) return 0;
    return 1;
}

// copies a name, cut to fit, without the characters the parts list and the type pick use (| = line breaks)
static void panel_copy_name(char *to, const char *from) {
    i32 length = 0;
    for (; *from && length < ENGINE_NAME_SIZE - 1; from++)
        if (*from != '|' && *from != '=' && *from != '\n' && *from != '\r') to[length++] = *from;
    to[length] = 0;
}

// emitters 0: actuator types, 1: emitter types. -1 = no type has that name
static i32 panel_find_type(Engine *engine, i32 emitters, const char *name) {
    i32 count = emitters ? engine->emitter_type_count : engine->actuator_type_count;
    for (i32 i = 0; i < count; i++)
        if (panel_same(emitters ? engine->emitter_types[i].name : engine->actuator_types[i].name, name)) return i;
    return -1;
}

// base without a number at its end, then "base 2", "base 3", ... the first one no type has
static void panel_unique_name(Engine *engine, i32 emitters, const char *base, char *name) {
    char stem[ENGINE_NAME_SIZE];
    panel_copy_name(stem, base);
    i32 length = str_len(stem), end = length;
    while (end > 0 && stem[end - 1] >= '0' && stem[end - 1] <= '9') end--;
    if (end < length && end > 1 && stem[end - 1] == ' ') stem[end - 1] = 0; // "side thrust 2" -> "side thrust"
    panel_copy_name(name, stem);
    for (i32 number = 2; panel_find_type(engine, emitters, name) >= 0 && number < 1000; number++) {
        char suffix[16];
        suffix[0] = 0;
        str_append(suffix, sizeof suffix, " ");
        str_append_i32(suffix, sizeof suffix, number);
        panel_copy_name(name, stem);
        i32 room = ENGINE_NAME_SIZE - 1 - str_len(suffix);
        if (str_len(name) > room) name[room] = 0;
        str_append(name, ENGINE_NAME_SIZE, suffix);
    }
}

// -1 = no object has that name
static i32 panel_find_object(Engine *engine, const char *name) {
    for (i32 i = 0; i < engine->object_count; i++)
        if (panel_same(engine->objects[i].name, name)) return i;
    return -1;
}

// the emits pick: 0 = sparks, 1 = sparks once, k = the (k - 1)-th copies_only object. -1 = none that far
static i32 panel_emitted_object(Engine *engine, i32 pick) {
    pick -= 1;
    for (i32 i = 0; i < engine->object_count; i++)
        if (engine->objects[i].copies_only && --pick == 0) return i;
    return -1;
}

#define PANEL_BURST_NAME "sparks once" // the emits pick's name for a burst type, also in the parts list ("explosion=sparks once")
#define PANEL_BURST_SPARKS 300         // a type turned into a burst in the panel starts with this many

static i32 panel_actuator_type_users(Engine *engine, i32 type) {
    i32 users = 0;
    for (i32 i = 0; i < engine->actuator_count; i++) users += engine->actuators[i].type == type;
    return users;
}

static i32 panel_emitter_type_users(Engine *engine, i32 type) {
    i32 users = 0;
    for (i32 i = 0; i < engine->emitter_count; i++) users += engine->emitters[i].type == type;
    for (i32 i = 0; i < engine->object_count; i++) users += engine->objects[i].hit_burst == type + 1;
    return users;
}

// starting numbers for a new type
static ActuatorType panel_new_actuator_type(i32 kind) {
    if (kind == ACTUATOR_TURN) return (ActuatorType){.kind = ACTUATOR_TURN, .speed = 2, .response = 10};
    return (ActuatorType){.kind = ACTUATOR_PUSH, .force = 30000, .cutoff = 10, .response = 4};
}

static EmitterType panel_new_emitter_type(void) {
    return (EmitterType){
        .emits = EMITS_SPARKS, .object = -1, .rate = 5000, .speed = 30, .speed_spread = 5, .base_speed = 30, .cone = 0.15f, .cone_squeeze = 0.3f,
        .lifetime = 1, .lifetime_spread = 1, .size = 0.05f, .color_start = {1.0f, 0.9f, 0.6f},
        .color_end = {1.0f, 0.3f, 0.05f}, .stretch = 0, .drag = 1,
    };
}

// adds a type with a name no other has (base, or base 2, ...). -1 = the table is full
static i32 panel_add_actuator_type(Engine *engine, ActuatorType type, const char *base) {
    if (engine->actuator_type_count >= ENGINE_MAX_ACTUATOR_TYPES) return -1;
    panel_unique_name(engine, 0, base, type.name);
    engine->actuator_types[engine->actuator_type_count] = type;
    return engine->actuator_type_count++;
}

static i32 panel_add_emitter_type(Engine *engine, EmitterType type, const char *base) {
    if (engine->emitter_type_count >= ENGINE_MAX_EMITTER_TYPES) return -1;
    panel_unique_name(engine, 1, base, type.name);
    engine->emitter_types[engine->emitter_type_count] = type;
    return engine->emitter_type_count++;
}

// the types after it move up one place, the parts that use them follow
static void panel_delete_actuator_type(Engine *engine, i32 type) {
    for (i32 i = type; i < engine->actuator_type_count - 1; i++) engine->actuator_types[i] = engine->actuator_types[i + 1];
    engine->actuator_type_count--;
    for (i32 i = 0; i < engine->actuator_count; i++)
        if (engine->actuators[i].type > type) engine->actuators[i].type--;
}

static void panel_delete_emitter_type(Engine *engine, i32 type) {
    for (i32 i = type; i < engine->emitter_type_count - 1; i++) engine->emitter_types[i] = engine->emitter_types[i + 1];
    engine->emitter_type_count--;
    for (i32 i = 0; i < engine->emitter_count; i++)
        if (engine->emitters[i].type > type) engine->emitters[i].type--;
    for (i32 i = 0; i < engine->object_count; i++)
        if (engine->objects[i].hit_burst > type + 1) engine->objects[i].hit_burst--;
}

// the typed name, if it is not empty and no other type has it
static void panel_rename(Engine *engine, i32 emitters, i32 type) {
    char name[ENGINE_NAME_SIZE];
    panel_copy_name(name, engine->panel_name);
    if (!name[0]) return;
    i32 other = panel_find_type(engine, emitters, name);
    if (other >= 0 && other != type) return;
    panel_copy_name(emitters ? engine->emitter_types[type].name : engine->actuator_types[type].name, name);
}

static void panel_do(Engine *engine, i32 what, i32 index, i32 option) {
    switch (what) {
    case PANEL_NEW_ACTUATOR_TYPE:
        panel_add_actuator_type(engine, panel_new_actuator_type(option), option == ACTUATOR_TURN ? "new turn" : "new push");
        break;
    case PANEL_COPY_ACTUATOR_TYPE:
        if (index < engine->actuator_type_count)
            panel_add_actuator_type(engine, engine->actuator_types[index], engine->actuator_types[index].name);
        break;
    case PANEL_DELETE_ACTUATOR_TYPE:
        if (index < engine->actuator_type_count && panel_actuator_type_users(engine, index) == 0)
            panel_delete_actuator_type(engine, index);
        break;
    case PANEL_RENAME_ACTUATOR_TYPE:
        if (index < engine->actuator_type_count) panel_rename(engine, 0, index);
        break;
    case PANEL_SPLIT_ACTUATOR:
        if (index < engine->actuator_count) {
            Actuator *actuator = &engine->actuators[index];
            ActuatorType *type = &engine->actuator_types[actuator->type];
            i32 copy = panel_add_actuator_type(engine, *type, type->name);
            if (copy >= 0) actuator->type = copy;
        }
        break;
    case PANEL_NEW_EMITTER_TYPE:
        panel_add_emitter_type(engine, panel_new_emitter_type(), "new emitter");
        break;
    case PANEL_COPY_EMITTER_TYPE:
        if (index < engine->emitter_type_count)
            panel_add_emitter_type(engine, engine->emitter_types[index], engine->emitter_types[index].name);
        break;
    case PANEL_DELETE_EMITTER_TYPE:
        if (index < engine->emitter_type_count && panel_emitter_type_users(engine, index) == 0)
            panel_delete_emitter_type(engine, index);
        break;
    case PANEL_RENAME_EMITTER_TYPE:
        if (index < engine->emitter_type_count) panel_rename(engine, 1, index);
        break;
    case PANEL_SPLIT_EMITTER:
        if (index < engine->emitter_count) {
            Emitter *emitter = &engine->emitters[index];
            EmitterType *type = &engine->emitter_types[emitter->type];
            i32 copy = panel_add_emitter_type(engine, *type, type->name);
            if (copy >= 0) emitter->type = copy;
        }
        break;
    case PANEL_PICK_EMITTER_TYPE:
        if (index < engine->emitter_count && option < engine->emitter_type_count) engine->emitters[index].type = option;
        break;
    case PANEL_PLACE_AGAIN:
        if (index < engine->placing_count) placing_place(engine, index);
        break;
    case PANEL_PICK_EMITS:
        if (index < engine->emitter_type_count) {
            EmitterType *type = &engine->emitter_types[index];
            i32 object = panel_emitted_object(engine, option);
            type->emits = object >= 0 ? EMITS_OBJECTS : EMITS_SPARKS;
            if (object >= 0) type->object = object;
            if (option != 1) type->burst = 0;
            else if (type->burst <= 0) type->burst = PANEL_BURST_SPARKS;
        }
        break;
    case PANEL_PICK_HIT_BURST:
        if (index < engine->object_count && option <= engine->emitter_type_count) engine->objects[index].hit_burst = option;
        break;
    }
}

// ---- The parts list as text

static void panel_write_parts(Engine *engine) {
    char *text = engine->panel_parts;
    i32 capacity = ENGINE_PARTS_TEXT_SIZE;
    text[0] = 0;
    for (i32 i = 0; i < engine->actuator_type_count; i++) {
        ActuatorType *type = &engine->actuator_types[i];
        str_append(text, capacity, type->kind == ACTUATOR_TURN ? "actuator turn " : "actuator push ");
        str_append(text, capacity, type->name);
        str_append(text, capacity, "\n");
    }
    for (i32 i = 0; i < engine->emitter_type_count; i++) {
        EmitterType *type = &engine->emitter_types[i];
        str_append(text, capacity, "emitter ");
        str_append(text, capacity, type->name);
        if (type->emits == EMITS_OBJECTS) {
            str_append(text, capacity, "=");
            str_append(text, capacity, engine->objects[type->object].name);
        } else if (type->burst > 0) {
            str_append(text, capacity, "=" PANEL_BURST_NAME);
        }
        str_append(text, capacity, "\n");
    }
    for (i32 i = 0; i < engine->actuator_count; i++) {
        str_append(text, capacity, "actuator use ");
        str_append(text, capacity, engine->actuators[i].name);
        str_append(text, capacity, "=");
        str_append(text, capacity, engine->actuator_types[engine->actuators[i].type].name);
        str_append(text, capacity, "\n");
    }
    for (i32 i = 0; i < engine->emitter_count; i++) {
        str_append(text, capacity, "emitter use ");
        str_append(text, capacity, engine->emitters[i].name);
        str_append(text, capacity, "=");
        str_append(text, capacity, engine->emitter_types[engine->emitters[i].type].name);
        str_append(text, capacity, "\n");
    }
    for (i32 i = 0; i < engine->object_count; i++) { // "object hit bullet=explosion": what bursts where its copies hit
        Object *object = &engine->objects[i];
        if (!object->copies_only) continue;
        str_append(text, capacity, "object hit ");
        str_append(text, capacity, object->name);
        str_append(text, capacity, "=");
        if (object->hit_burst > 0 && object->hit_burst <= engine->emitter_type_count)
            str_append(text, capacity, engine->emitter_types[object->hit_burst - 1].name);
        str_append(text, capacity, "\n");
    }
}

// the next line of text into line (cut to fit), returns where the one after starts
static const char *panel_next_line(const char *text, char *line, i32 capacity) {
    i32 length = 0;
    for (; *text && *text != '\n'; text++)
        if (length < capacity - 1) line[length++] = *text;
    line[length] = 0;
    return *text ? text + 1 : text;
}

// "name=type": splits at the =, -1 = there is none
static i32 panel_split_use(char *rest) {
    for (i32 i = 0; rest[i]; i++)
        if (rest[i] == '=') { rest[i] = 0; return i + 1; }
    return -1;
}

// Builds the type tables the list names, in its order: a type that is already there keeps its numbers (found by name),
// a new name starts with the starting numbers. A type the list leaves out but a part still uses stays, at the end.
// Then each part takes the type the list gives it, by name. A list without any type is ignored (empty or broken)
static void panel_apply_parts(Engine *engine) {
    static ActuatorType actuator_types[ENGINE_MAX_ACTUATOR_TYPES];
    static EmitterType emitter_types[ENGINE_MAX_EMITTER_TYPES];
    i32 actuator_type_count = 0, emitter_type_count = 0;
    i32 actuator_type_now[ENGINE_MAX_ACTUATOR_TYPES], emitter_type_now[ENGINE_MAX_EMITTER_TYPES]; // old place -> new
    for (i32 i = 0; i < ENGINE_MAX_ACTUATOR_TYPES; i++) actuator_type_now[i] = -1;
    for (i32 i = 0; i < ENGINE_MAX_EMITTER_TYPES; i++) emitter_type_now[i] = -1;

    char line[128];
    for (const char *text = engine->panel_parts; *text;) {
        text = panel_next_line(text, line, sizeof line);
        if (panel_starts_with(line, "actuator use ") || panel_starts_with(line, "emitter use ")) continue;
        if (panel_starts_with(line, "actuator push ") || panel_starts_with(line, "actuator turn ")) {
            if (actuator_type_count >= ENGINE_MAX_ACTUATOR_TYPES) continue;
            i32 kind = panel_starts_with(line, "actuator turn ") ? ACTUATOR_TURN : ACTUATOR_PUSH;
            const char *name = line + 14;
            i32 old = panel_find_type(engine, 0, name);
            ActuatorType type = old >= 0 ? engine->actuator_types[old] : panel_new_actuator_type(kind);
            type.kind = kind;
            panel_copy_name(type.name, name);
            if (old >= 0) actuator_type_now[old] = actuator_type_count;
            actuator_types[actuator_type_count++] = type;
        } else if (panel_starts_with(line, "emitter ")) {
            if (emitter_type_count >= ENGINE_MAX_EMITTER_TYPES) continue;
            char *name = line + 8;
            i32 object_at = panel_split_use(name); // "machine gun=bullet": sends out copies of bullet
            i32 old = panel_find_type(engine, 1, name);
            EmitterType type = old >= 0 ? engine->emitter_types[old] : panel_new_emitter_type();
            panel_copy_name(type.name, name);
            i32 object = object_at >= 0 ? panel_find_object(engine, name + object_at) : -1;
            type.emits = object >= 0 && engine->objects[object].copies_only ? EMITS_OBJECTS : EMITS_SPARKS;
            if (type.emits == EMITS_OBJECTS) type.object = object;
            i32 burst = object_at >= 0 && panel_same(name + object_at, PANEL_BURST_NAME); // "explosion=sparks once"
            type.burst = !burst ? 0 : type.burst > 0 ? type.burst : PANEL_BURST_SPARKS;
            if (old >= 0) emitter_type_now[old] = emitter_type_count;
            emitter_types[emitter_type_count++] = type;
        }
    }
    if (actuator_type_count == 0 && emitter_type_count == 0) return;

    for (i32 i = 0; i < engine->actuator_count; i++) { // parts keep their type, at its new place
        Actuator *actuator = &engine->actuators[i];
        if (actuator_type_now[actuator->type] < 0 && actuator_type_count < ENGINE_MAX_ACTUATOR_TYPES) {
            actuator_type_now[actuator->type] = actuator_type_count;
            actuator_types[actuator_type_count++] = engine->actuator_types[actuator->type];
        }
        actuator->type = actuator_type_now[actuator->type] >= 0 ? actuator_type_now[actuator->type] : 0;
    }
    for (i32 i = 0; i < engine->emitter_count; i++) {
        Emitter *emitter = &engine->emitters[i];
        if (emitter_type_now[emitter->type] < 0 && emitter_type_count < ENGINE_MAX_EMITTER_TYPES) {
            emitter_type_now[emitter->type] = emitter_type_count;
            emitter_types[emitter_type_count++] = engine->emitter_types[emitter->type];
        }
        emitter->type = emitter_type_now[emitter->type] >= 0 ? emitter_type_now[emitter->type] : 0;
    }
    for (i32 i = 0; i < engine->object_count; i++) { // and the burst an object leaves where its copies hit
        Object *object = &engine->objects[i];
        if (object->hit_burst <= 0) continue;
        i32 old = object->hit_burst - 1;
        if (emitter_type_now[old] < 0 && emitter_type_count < ENGINE_MAX_EMITTER_TYPES) {
            emitter_type_now[old] = emitter_type_count;
            emitter_types[emitter_type_count++] = engine->emitter_types[old];
        }
        object->hit_burst = emitter_type_now[old] >= 0 ? emitter_type_now[old] + 1 : 0;
    }
    for (i32 i = 0; i < actuator_type_count; i++) engine->actuator_types[i] = actuator_types[i];
    for (i32 i = 0; i < emitter_type_count; i++) engine->emitter_types[i] = emitter_types[i];
    engine->actuator_type_count = actuator_type_count;
    engine->emitter_type_count = emitter_type_count;

    for (const char *text = engine->panel_parts; *text;) { // then which type each part uses
        text = panel_next_line(text, line, sizeof line);
        if (panel_starts_with(line, "object hit ")) { // "object hit bullet=explosion", nothing after = no burst
            char *rest = line + 11;
            i32 type_at = panel_split_use(rest);
            i32 object = panel_find_object(engine, rest);
            if (type_at < 0 || object < 0) continue;
            i32 type = rest[type_at] ? panel_find_type(engine, 1, rest + type_at) : -1;
            engine->objects[object].hit_burst = type + 1;
            continue;
        }
        i32 actuators = panel_starts_with(line, "actuator use ");
        if (!actuators && !panel_starts_with(line, "emitter use ")) continue;
        char *rest = line + (actuators ? 13 : 12);
        i32 type_at = panel_split_use(rest);
        if (type_at < 0) continue;
        i32 type = panel_find_type(engine, !actuators, rest + type_at);
        if (type < 0) continue;
        if (actuators) {
            for (i32 i = 0; i < engine->actuator_count; i++)
                if (panel_same(engine->actuators[i].name, rest)) engine->actuators[i].type = type;
        } else {
            for (i32 i = 0; i < engine->emitter_count; i++)
                if (panel_same(engine->emitters[i].name, rest)) engine->emitters[i].type = type;
        }
    }
}

i32 engine_update_panel(Engine *engine) {
    i32 changed = 0;
    for (i32 i = 0; i < engine->placing_count; i++) placing_update(engine, i); // a slider moved: placed again
    if (engine->panel_parts_changed) {
        engine->panel_parts_changed = 0;
        panel_apply_parts(engine);
        changed = 1;
    }
    i32 action = engine->panel_action;
    if (action) {
        engine->panel_action = PANEL_NONE;
        panel_do(engine, action >> 16, (action >> 8) & 255, action & 255);
        changed = 1;
    }
    return changed;
}

// ---- The tree

// "shared with" a list of the other parts using the same type, then split. or "not shared"
static void panel_add_sharing(const char *others, i32 split_action, i32 *target) {
    if (!others[0]) {
        plat_ui_row("not shared", "");
        return;
    }
    plat_ui_row("shared with", others);
    plat_ui_button("split", "", target, split_action);
}

void engine_add_panel(Engine *engine) {
    panel_write_parts(engine);
    plat_ui_text("parts", engine->panel_parts, ENGINE_PARTS_TEXT_SIZE, &engine->panel_parts_changed);
    char note[128], others[128], options[ENGINE_MAX_EMITTER_TYPES * ENGINE_NAME_SIZE];
    plat_ui_group("universe");
    plat_ui_fold("gravity", "everything pulls on everything, hits bounce");
    plat_ui_block("strength");
    plat_ui_slider_log("strength × real G", 1, 10000000, 1, &engine->gravity); // real G moves nothing at this scale
    plat_ui_block_end();
    plat_ui_block("bounce");
    plat_ui_slider("bounce", 0, 1, 0.05f, &engine->bounce);
    plat_ui_block_end();
    plat_ui_fold_end();
    plat_ui_fold("stars", "on the sky, look only");
    plat_ui_block("stars");
    plat_ui_slider("stars", 0, 20000, 100, &engine->stars.count);
    plat_ui_slider("size px", 0.5f, 8, 0.1f, &engine->stars.size);
    plat_ui_block_end();
    plat_ui_block("light");
    plat_ui_slider("brightness", 0, 3, 0.05f, &engine->stars.brightness);
    plat_ui_slider("twinkle", 0, 1, 0.05f, &engine->stars.twinkle);
    plat_ui_block_end();
    plat_ui_block("colors");
    plat_ui_color("base color", engine->stars.color);
    plat_ui_color("tint 1", engine->stars.tint_one);
    plat_ui_color("tint 2", engine->stars.tint_two);
    plat_ui_block_end();
    plat_ui_fold_end();
    plat_ui_fold("spread", "every placing puts its copies in this ball");
    plat_ui_block("spread");
    plat_ui_slider("spread m", 100, 50000, 100, &engine->spread);
    plat_ui_block_end();
    plat_ui_fold_end();
    for (i32 i = 0; i < engine->placing_count; i++) {
        Placing *placing = &engine->placings[i];
        note[0] = 0; // "places: planet"
        str_append(note, sizeof note, "places: ");
        str_append(note, sizeof note, placing->object >= 0 ? engine->objects[placing->object].name : "nothing");
        plat_ui_fold(placing->name, note);
        plat_ui_fold_button("again", &engine->panel_action, PANEL_ACTION(PANEL_PLACE_AGAIN, i));
        plat_ui_block("count");
        plat_ui_slider("count", 0, 5000, 1, &placing->count); // copies that pull: cost grows with their count x all
        plat_ui_block_end();
        plat_ui_block("size");
        plat_ui_slider("size min m", 1, 2000, 1, &placing->size_min);
        plat_ui_slider("size max m", 1, 2000, 1, &placing->size_max);
        plat_ui_slider("mostly small", 0, 1, 0.05f, &placing->mostly_small);
        plat_ui_block_end();
        plat_ui_block("speed");
        plat_ui_slider("speed min m/s", 0, 500, 0.5f, &placing->speed_min);
        plat_ui_slider("speed max m/s", 0, 500, 0.5f, &placing->speed_max);
        plat_ui_slider("small ones faster", 0, 1, 0.05f, &placing->small_faster);
        plat_ui_block_end();
        plat_ui_fold_end();
    }

    plat_ui_group("objects");
    for (i32 i = 0; i < engine->object_count; i++) {
        Object *object = &engine->objects[i];
        i32 placed = 0; // put down by a placing: mass from density, no drag, lives forever
        for (i32 j = 0; j < engine->placing_count; j++) placed |= engine->placings[j].object == i;
        plat_ui_fold(object->name, "");
        if (object->density > 0) {
            plat_ui_block("density");
            plat_ui_slider_log("density kg/m³", 1, 100000, 1, &object->density);
            plat_ui_block_end();
        } else {
            plat_ui_block("mass");
            plat_ui_slider_log("mass kg", 0.01f, 100000, 0.01f, &object->mass); // log: a bullet and a ship both fit
            plat_ui_block_end();
        }
        plat_ui_block("gravity");
        plat_ui_choice("pulls", "yes|no", &object->pull_off); // no: still pulled by the ones that pull, pulls nothing
        plat_ui_block_end();
        if (!placed) {
            plat_ui_block("drag");
            plat_ui_slider("drag /s", 0, 3, 0.01f, &object->drag);
            if (object->drag_cut_input.kind != SOURCE_NONE) plat_ui_slider("drag cut %", 0, 100, 1, &object->drag_cut);
            plat_ui_block_end();
        }
        if (object->copies_only && !placed) {
            plat_ui_block("life");
            plat_ui_slider("life s", 0.1f, 20, 0.1f, &object->lifetime);
            plat_ui_block_end();
        }
        plat_ui_block("model");
        if (!placed) plat_ui_slider("size m", 0.01f, 10, 0.01f, &object->size); // placed: the placing picks sizes
        plat_ui_color("color", &object->color.x);
        if (object->model == OBJECT_MODEL_BALL) { // its surface's other two colors. each copy mixes its own from the three
            plat_ui_color("color 2", object->surface.color_two);
            plat_ui_color("color 3", object->surface.color_three);
            plat_ui_choice("colors", "these three|10 sets", &object->surface.color_sets); // sets: each copy picks one
            plat_ui_slider("variation", 0, 1, 0.05f, &object->surface.variation);
        }
        plat_ui_choice("glow", "Off|On", &object->glow);
        plat_ui_slider("halo m", 0, 10, 0.05f, &object->halo); // only drawn while glow is on
        plat_ui_block_end();
        if (object->model == OBJECT_MODEL_BALL) {
            plat_ui_block("surface");
            plat_ui_choice("pattern", "patches|bands|both", &object->surface.pattern);
            plat_ui_slider("detail", 0.5f, 20, 0.1f, &object->surface.detail);
            plat_ui_slider("roughness", 0, 1, 0.05f, &object->surface.roughness);
            plat_ui_block_end();
            plat_ui_block("air");
            plat_ui_color("air color", object->surface.air_color);
            plat_ui_slider("thickness", 0, 2, 0.05f, &object->surface.air_thickness); // 0 = no rim
            plat_ui_block_end();
        }
        if (object->copies_only) { // a copy that hits something: a burst where it was, then gone
            options[0] = 0; // "nothing|main|side|...|explosion"
            str_append(options, sizeof options, "nothing");
            for (i32 t = 0; t < engine->emitter_type_count; t++) {
                str_append(options, sizeof options, "|");
                str_append(options, sizeof options, engine->emitter_types[t].name);
            }
            plat_ui_block("on hit");
            plat_ui_pick("burst", options, object->hit_burst, &engine->panel_action, PANEL_ACTION(PANEL_PICK_HIT_BURST, i));
            plat_ui_slider("gone after steps", 1, 30, 1, &object->hit_steps);
            plat_ui_choice("moves with", "what it hits|nothing", &object->hit_moves_with);
            plat_ui_slider("straight out", 0, 1, 0.05f, &object->hit_straight_out); // 0 = mirrored off the surface
            plat_ui_slider("speed from hit", 0, 1, 0.01f, &object->hit_speed_share); // × its speed into the surface
            plat_ui_block_end();
        }
        i32 actuator_count = 0, emitter_count = 0; // folds only for the ones it has
        for (i32 j = 0; j < engine->actuator_count; j++) actuator_count += engine->actuators[j].object == i;
        for (i32 j = 0; j < engine->emitter_count; j++) emitter_count += engine->emitters[j].object == i;
        if (!actuator_count && !emitter_count) {
            plat_ui_fold_end();
            continue;
        }
        plat_ui_fold("actuators", "");
        for (i32 j = 0; j < engine->actuator_count; j++) {
            Actuator *actuator = &engine->actuators[j];
            if (actuator->object != i) continue;
            note[0] = 0; // "engine <- W, boost Space"
            str_append(note, sizeof note, engine->actuator_types[actuator->type].name);
            str_append(note, sizeof note, " ← ");
            source_describe(engine, actuator->input, note, sizeof note);
            if (actuator->boost.kind != SOURCE_NONE) {
                str_append(note, sizeof note, ", boost ");
                source_describe(engine, actuator->boost, note, sizeof note);
            }
            plat_ui_fold(actuator->name, note);
            if (actuator->fallback.kind != SOURCE_NONE) { // "when idle" nothing | level
                options[0] = 0;
                str_append(options, sizeof options, "nothing|");
                source_describe(engine, actuator->fallback, options, sizeof options);
                plat_ui_choice("when idle", options, &actuator->fallback_on);
            }
            others[0] = 0;
            for (i32 k = 0; k < engine->actuator_count; k++) {
                if (k == j || engine->actuators[k].type != actuator->type) continue;
                if (others[0]) str_append(others, sizeof others, ", ");
                str_append(others, sizeof others, engine->actuators[k].name);
            }
            panel_add_sharing(others, PANEL_ACTION(PANEL_SPLIT_ACTUATOR, j), &engine->panel_action);
            plat_ui_fold_end();
        }
        plat_ui_fold_end();
        options[0] = 0; // the type pick: every emitter type, "main|side"
        for (i32 t = 0; t < engine->emitter_type_count; t++) {
            if (t) str_append(options, sizeof options, "|");
            str_append(options, sizeof options, engine->emitter_types[t].name);
        }
        plat_ui_fold("emitters", "");
        for (i32 j = 0; j < engine->emitter_count; j++) {
            Emitter *emitter = &engine->emitters[j];
            if (emitter->object != i) continue;
            note[0] = 0; // "main <- engine"
            str_append(note, sizeof note, engine->emitter_types[emitter->type].name);
            str_append(note, sizeof note, " ← ");
            source_describe(engine, emitter->input, note, sizeof note);
            plat_ui_fold(emitter->name, note);
            plat_ui_pick("type", options, emitter->type, &engine->panel_action, PANEL_ACTION(PANEL_PICK_EMITTER_TYPE, j));
            others[0] = 0;
            for (i32 k = 0; k < engine->emitter_count; k++) {
                if (k == j || engine->emitters[k].type != emitter->type) continue;
                if (others[0]) str_append(others, sizeof others, ", ");
                str_append(others, sizeof others, engine->emitters[k].name);
            }
            panel_add_sharing(others, PANEL_ACTION(PANEL_SPLIT_EMITTER, j), &engine->panel_action);
            plat_ui_fold_end();
        }
        plat_ui_fold_end();
        plat_ui_fold_end();
    }

    plat_ui_group("actuators");
    for (i32 i = 0; i < engine->actuator_type_count; i++) {
        ActuatorType *type = &engine->actuator_types[i];
        plat_ui_fold(type->name, type->kind == ACTUATOR_TURN ? "turn" : "push");
        plat_ui_fold_rename(engine->panel_name, ENGINE_NAME_SIZE, &engine->panel_action, PANEL_ACTION(PANEL_RENAME_ACTUATOR_TYPE, i));
        plat_ui_fold_button("copy", &engine->panel_action, PANEL_ACTION(PANEL_COPY_ACTUATOR_TYPE, i));
        if (panel_actuator_type_users(engine, i) == 0)
            plat_ui_fold_button("×", &engine->panel_action, PANEL_ACTION(PANEL_DELETE_ACTUATOR_TYPE, i));
        if (type->kind == ACTUATOR_TURN) {
            plat_ui_slider("speed rad/s", 0.1f, 8, 0.05f, &type->speed);
            plat_ui_slider("response /s", 0.5f, 30, 0.1f, &type->response);
        } else {
            plat_ui_slider("force N", 0, 100000, 500, &type->force);
            i32 boosted = 0; // only when an actuator of this type has a boost input
            for (i32 j = 0; j < engine->actuator_count; j++)
                if (engine->actuators[j].type == i && engine->actuators[j].boost.kind != SOURCE_NONE) boosted = 1;
            if (boosted) plat_ui_slider("boost %", 0, 300, 1, &type->boost);
            plat_ui_slider("response /s", 0.5f, 30, 0.1f, &type->response);
            plat_ui_slider("cutoff %", 0, 50, 1, &type->cutoff);
        }
        plat_ui_fold_end();
    }
    if (engine->actuator_type_count < ENGINE_MAX_ACTUATOR_TYPES) // option = ACTUATOR_PUSH or ACTUATOR_TURN
        plat_ui_button("+ new", "push|turn", &engine->panel_action, PANEL_ACTION(PANEL_NEW_ACTUATOR_TYPE, 0));

    options[0] = 0; // the emits pick: "sparks|sparks once|bullet", sparks, a burst, then every copies_only object
    str_append(options, sizeof options, "sparks|" PANEL_BURST_NAME);
    for (i32 k = 0; k < engine->object_count; k++) {
        if (!engine->objects[k].copies_only) continue;
        str_append(options, sizeof options, "|");
        str_append(options, sizeof options, engine->objects[k].name);
    }
    plat_ui_group("emitters");
    for (i32 i = 0; i < engine->emitter_type_count; i++) {
        EmitterType *type = &engine->emitter_types[i];
        i32 picked = type->burst > 0 ? 1 : 0; // sparks, sparks once, or the emitted object's place in the pick
        if (type->emits == EMITS_OBJECTS) {
            picked = 1;
            for (i32 k = 0; k <= type->object && k < engine->object_count; k++) picked += engine->objects[k].copies_only;
        }
        note[0] = 0; // "emits: sparks"
        str_append(note, sizeof note, "emits: ");
        str_append(note, sizeof note, type->emits == EMITS_OBJECTS ? engine->objects[type->object].name
                                      : type->burst > 0             ? PANEL_BURST_NAME
                                                                    : "sparks");
        plat_ui_fold(type->name, note);
        plat_ui_fold_rename(engine->panel_name, ENGINE_NAME_SIZE, &engine->panel_action, PANEL_ACTION(PANEL_RENAME_EMITTER_TYPE, i));
        plat_ui_fold_button("copy", &engine->panel_action, PANEL_ACTION(PANEL_COPY_EMITTER_TYPE, i));
        if (panel_emitter_type_users(engine, i) == 0)
            plat_ui_fold_button("×", &engine->panel_action, PANEL_ACTION(PANEL_DELETE_EMITTER_TYPE, i));
        plat_ui_pick("emits", options, picked, &engine->panel_action, PANEL_ACTION(PANEL_PICK_EMITS, i));
        emitter_type_add_sliders(type);
        plat_ui_fold_end();
    }
    if (engine->emitter_type_count < ENGINE_MAX_EMITTER_TYPES)
        plat_ui_button("+ new", "", &engine->panel_action, PANEL_ACTION(PANEL_NEW_EMITTER_TYPE, 0));
}
