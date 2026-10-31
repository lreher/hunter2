// WebGPU bridge. C decides what to create and draw, this file only does it.
// Real objects live in a table, C gets integer handles (0 is never valid).
// Enum values and descriptor layouts must match src/platform/gpu.h.
import { fail } from "./env.js";

const decoder = new TextDecoder();

// GpuPipelineDesc and GpuBindGroupDesc as i32 counts
const MAX_ATTRIBUTES = 8;
const MAX_BIND_ENTRIES = 8;
const PIPELINE_DESC_INTS = 7 + MAX_ATTRIBUTES * 3;
const BIND_GROUP_DESC_INTS = 3 + MAX_BIND_ENTRIES * 3;

const VERTEX_FORMATS = ["float32x2", "float32x3", "float32x4"];
const STEP_MODES = ["vertex", "instance"];
const TOPOLOGIES = ["triangle-list", "triangle-strip"];
const FILTERS = ["nearest", "linear"];
const BLENDS = [
    undefined,
    {
        color: { srcFactor: "src-alpha", dstFactor: "one-minus-src-alpha" },
        alpha: { srcFactor: "one", dstFactor: "one-minus-src-alpha" },
    },
    {
        color: { srcFactor: "src-alpha", dstFactor: "one" },
        alpha: { srcFactor: "one", dstFactor: "one" },
    },
];

// look up an enum value, fail loudly if C sent something we do not know
function pick(table, index, what) {
    if (index < 0 || index >= table.length) fail(`gpu: unknown ${what} ${index}`);
    return table[index];
}

// returns the js_gpu_ functions C calls
export async function createGpu(ctx) {
    const canvas = ctx.canvas;

    if (!navigator.gpu) fail("WebGPU not supported in this browser");
    const adapter = await navigator.gpu.requestAdapter();
    if (!adapter) fail("no WebGPU adapter");
    const device = await adapter.requestDevice();
    device.lost.then((info) => console.error("[js] gpu device lost:", info.message));
    device.addEventListener("uncapturederror", (e) => console.error("[js] gpu error:", e.error.message));

    const context = canvas.getContext("webgpu");
    const screenFormat = navigator.gpu.getPreferredCanvasFormat();
    context.configure({ device, format: screenFormat, alphaMode: "opaque" });

    // the canvas texture follows canvas.width/height, C reads them every frame
    function resize() {
        canvas.width = Math.max(1, Math.floor(canvas.clientWidth * devicePixelRatio));
        canvas.height = Math.max(1, Math.floor(canvas.clientHeight * devicePixelRatio));
    }
    resize();
    window.addEventListener("resize", resize);
    console.log("[js] webgpu ready", screenFormat, `${canvas.width}x${canvas.height}`);

    const TEXTURE_FORMATS = ["r8unorm", "rgba8unorm", screenFormat];

    // handle table
    const objects = [null];
    function add(kind, obj) {
        objects.push({ kind, obj });
        const handle = objects.length - 1;
        console.log(`[js] gpu create ${kind} #${handle}`);
        return handle;
    }
    function get(handle, kind) {
        const entry = objects[handle];
        if (!entry) fail(`gpu: bad handle ${handle} (wanted ${kind})`);
        if (entry.kind !== kind) fail(`gpu: handle ${handle} is a ${entry.kind}, wanted ${kind}`);
        return entry.obj;
    }

    function readDesc(ptr, size, ints, name) {
        if (size !== ints * 4) fail(`gpu: ${name} is ${size} bytes in C, JS expects ${ints * 4}`);
        return new Int32Array(ctx.memory.buffer, ptr, ints);
    }

    // frame state, set between begin_frame and end_frame
    let encoder = null;
    let pass = null;
    function needPass(what) {
        if (!pass) fail(`gpu: ${what} called outside begin_frame/end_frame`);
    }

    return {
        js_gpu_create_shader(ptr, len) {
            const code = decoder.decode(new Uint8Array(ctx.memory.buffer, ptr, len));
            const module = device.createShaderModule({ code });
            module.getCompilationInfo().then((info) => {
                for (const m of info.messages) {
                    console.error(`[js] shader ${m.type} line ${m.lineNum}: ${m.message}`);
                }
            });
            return add("shader", module);
        },

        js_gpu_create_buffer(size, usage) {
            return add("buffer", device.createBuffer({ size, usage }));
        },

        js_gpu_create_texture(w, h, format, usage) {
            const texture = device.createTexture({
                size: [w, h],
                format: pick(TEXTURE_FORMATS, format, "texture format"),
                usage,
            });
            return add("texture", texture);
        },

        js_gpu_create_sampler(filter) {
            const mode = pick(FILTERS, filter, "filter");
            return add("sampler", device.createSampler({ magFilter: mode, minFilter: mode }));
        },

        js_gpu_create_pipeline(ptr, size) {
            const d = readDesc(ptr, size, PIPELINE_DESC_INTS, "GpuPipelineDesc");
            const [shader, stride, step, attributeCount, targetFormat, blend, topology] = d;

            const vertex = { module: get(shader, "shader"), entryPoint: "vs" };
            if (stride > 0) {
                const attributes = [];
                for (let i = 0; i < attributeCount; i++) {
                    const a = 7 + i * 3;
                    attributes.push({
                        shaderLocation: d[a],
                        offset: d[a + 1],
                        format: pick(VERTEX_FORMATS, d[a + 2], "vertex format"),
                    });
                }
                vertex.buffers = [{
                    arrayStride: stride,
                    stepMode: pick(STEP_MODES, step, "step mode"),
                    attributes,
                }];
            }

            const target = { format: pick(TEXTURE_FORMATS, targetFormat, "target format") };
            const blendState = pick(BLENDS, blend, "blend");
            if (blendState) target.blend = blendState;

            const pipeline = device.createRenderPipeline({
                layout: "auto",
                vertex,
                fragment: { module: get(shader, "shader"), entryPoint: "fs", targets: [target] },
                primitive: { topology: pick(TOPOLOGIES, topology, "topology") },
            });
            return add("pipeline", pipeline);
        },

        js_gpu_create_bind_group(ptr, size) {
            const d = readDesc(ptr, size, BIND_GROUP_DESC_INTS, "GpuBindGroupDesc");
            const [pipeline, group, entryCount] = d;

            const entries = [];
            for (let i = 0; i < entryCount; i++) {
                const e = 3 + i * 3;
                const binding = d[e];
                const type = d[e + 1];
                const handle = d[e + 2];
                if (type === 0) entries.push({ binding, resource: { buffer: get(handle, "buffer") } });
                else if (type === 1) entries.push({ binding, resource: get(handle, "texture").createView() });
                else if (type === 2) entries.push({ binding, resource: get(handle, "sampler") });
                else fail(`gpu: unknown bind type ${type}`);
            }

            const layout = get(pipeline, "pipeline").getBindGroupLayout(group);
            return add("bind group", device.createBindGroup({ layout, entries }));
        },

        js_gpu_destroy(handle) {
            const entry = objects[handle];
            if (!entry) fail(`gpu: destroy of bad handle ${handle}`);
            if (entry.obj.destroy) entry.obj.destroy();
            objects[handle] = null;
            console.log(`[js] gpu destroy ${entry.kind} #${handle}`);
        },

        js_gpu_write_buffer(handle, offset, ptr, size) {
            device.queue.writeBuffer(get(handle, "buffer"), offset, ctx.memory.buffer, ptr, size);
        },

        js_gpu_write_texture(handle, ptr, w, h, bytesPerRow) {
            const data = new Uint8Array(ctx.memory.buffer, ptr, bytesPerRow * h);
            device.queue.writeTexture({ texture: get(handle, "texture") }, data, { bytesPerRow }, [w, h]);
        },

        js_gpu_begin_frame(r, g, b) {
            if (pass) fail("gpu: begin_frame called twice");
            encoder = device.createCommandEncoder();
            pass = encoder.beginRenderPass({
                colorAttachments: [{
                    view: context.getCurrentTexture().createView(),
                    loadOp: "clear",
                    storeOp: "store",
                    clearValue: { r, g, b, a: 1 },
                }],
            });
        },

        js_gpu_set_pipeline(handle) {
            needPass("set_pipeline");
            pass.setPipeline(get(handle, "pipeline"));
        },

        js_gpu_set_bind_group(group, handle) {
            needPass("set_bind_group");
            pass.setBindGroup(group, get(handle, "bind group"));
        },

        js_gpu_set_vertex_buffer(slot, handle, offset) {
            needPass("set_vertex_buffer");
            pass.setVertexBuffer(slot, get(handle, "buffer"), offset);
        },

        js_gpu_draw(vertexCount, instanceCount) {
            needPass("draw");
            pass.draw(vertexCount, instanceCount);
        },

        js_gpu_end_frame() {
            needPass("end_frame");
            pass.end();
            device.queue.submit([encoder.finish()]);
            pass = null;
            encoder = null;
        },
    };
}
