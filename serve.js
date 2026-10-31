// tiny static server for web/. usage: node serve.js [port]
const http = require("http");
const fs = require("fs");
const path = require("path");

const root = path.join(__dirname, "web");
const port = Number(process.argv[2] ?? 8000);
const types = {
    ".html": "text/html",
    ".js": "text/javascript",
    ".wasm": "application/wasm", // needed for instantiateStreaming
    ".jpg": "image/jpeg",
    ".png": "image/png",
};

http.createServer((req, res) => {
    const url = req.url.split("?")[0];
    const file = path.join(root, url === "/" ? "index.html" : url);
    if (!file.startsWith(root)) {
        res.writeHead(403).end();
        return;
    }
    fs.readFile(file, (err, data) => {
        const status = err ? 404 : 200;
        console.log(status, req.url);
        if (err) {
            res.writeHead(404).end();
            return;
        }
        const type = types[path.extname(file)] ?? "application/octet-stream";
        res.writeHead(200, { "Content-Type": type, "Cache-Control": "no-store" }).end(data);
    });
}).listen(port, () => console.log(`serving web/ at http://localhost:${port}`));
