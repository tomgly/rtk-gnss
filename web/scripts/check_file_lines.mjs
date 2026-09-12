import { readdir, readFile } from "node:fs/promises";
import { extname, join } from "node:path";

const allowed = new Set([".astro", ".ts", ".tsx", ".js", ".mjs", ".css"]);
const violations = [];

async function walk(dir) {
  for (const entry of await readdir(dir, { withFileTypes: true })) {
    const path = join(dir, entry.name);
    if (entry.isDirectory()) await walk(path);
    else if (entry.name !== "worker-configuration.d.ts" && allowed.has(extname(path))) {
      const lines = (await readFile(path, "utf8")).split("\n").length;
      if (lines > 500) violations.push(`${path}: ${lines} lines`);
    }
  }
}

await walk("src");
if (violations.length) {
  console.error(violations.join("\n"));
  process.exit(1);
}
console.log("Source line limits OK");
