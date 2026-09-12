import type { APIContext } from "astro";
import { env } from "cloudflare:workers";

export function requireDevice(context: APIContext) {
	const auth = context.request.headers.get("authorization") ?? "";
	const expected = `Bearer ${env.DEVICE_API_TOKEN}`;
	if (auth === expected) return null;
	return new Response(JSON.stringify({ error: "unauthorized" }), {
		status: 401,
		headers: { "content-type": "application/json" },
	});
}
