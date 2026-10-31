// Loads tests/test.wasm and runs it. Exit code = number of failures.
const fs = require("fs");
const path = require("path");

const env = {
    t_fail(ptr, len, line) {
        const expr = new TextDecoder().decode(new Uint8Array(memory.buffer, ptr, len));
        console.log(`FAIL line ${line}: ${expr}`);
    },
};
let memory;
WebAssembly.instantiate(fs.readFileSync(path.join(__dirname, "test.wasm")), { env }).then(({ instance }) => {
    memory = instance.exports.memory;
    const failures = instance.exports.run_tests();
    console.log(failures === 0 ? "all tests passed" : `${failures} checks failed`);
    process.exit(failures === 0 ? 0 : 1);
});
