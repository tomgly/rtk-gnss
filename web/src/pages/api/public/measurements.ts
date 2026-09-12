import type { APIRoute } from "astro";
import { env } from "cloudflare:workers";
export const GET: APIRoute = async () => {
	const rows = await env.DB.prepare(
		"SELECT * FROM measurements ORDER BY gateway_time DESC LIMIT 200",
	).all();
	return new Response(JSON.stringify({ measurements: rows.results ?? [] }), {
		headers: { "content-type": "application/json" },
	});
};
