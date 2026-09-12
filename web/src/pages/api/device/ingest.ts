import type { APIRoute } from "astro";
import { env } from "cloudflare:workers";
import { requireDevice } from "../../../lib/device-auth";
import { ingest } from "../../../lib/ingest";

export const POST: APIRoute = async (context) => {
	const denied = requireDevice(context);
	if (denied) return denied;
	try {
		const body = await context.request.json();
		if (!body || typeof body !== "object" || Array.isArray(body))
			throw new Error("invalid payload");
		const result = await ingest(env.DB, body as Record<string, unknown>);
		return new Response(JSON.stringify(result), {
			headers: { "content-type": "application/json" },
		});
	} catch (error) {
		return new Response(
			JSON.stringify({
				error: error instanceof Error ? error.message : "invalid payload",
			}),
			{
				status: 400,
				headers: { "content-type": "application/json" },
			},
		);
	}
};
