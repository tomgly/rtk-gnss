import type { APIRoute } from "astro";
import { env } from "cloudflare:workers";
export const GET: APIRoute = async () => {
	const rows = await env.DB.prepare(
		"SELECT * FROM sessions ORDER BY created_at DESC LIMIT 100",
	).all();
	return new Response(JSON.stringify({ sessions: rows.results ?? [] }), {
		headers: { "content-type": "application/json" },
	});
};
