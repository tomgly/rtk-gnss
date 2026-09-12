import type { APIRoute } from "astro";
import { env } from "cloudflare:workers";
import { requireAuthentication } from "../../../lib/auth";
import { DEFAULT_GATEWAY_ID } from "../../../lib/config";
import { newId } from "../../../lib/ids";

export const POST: APIRoute = async (context) => {
	const denied = await requireAuthentication(context);
	if (denied) return denied;
	const body = (await context.request.json().catch(() => ({}))) as {
		gateway_id?: unknown;
		name?: unknown;
	};
	const name = String(body.name || "").trim();
	if (!name)
		return new Response(JSON.stringify({ error: "name required" }), {
			status: 400,
		});
	const db = env.DB;
	const current = await db
		.prepare(
			"SELECT session_id FROM sessions WHERE active=1 ORDER BY started_at DESC LIMIT 1",
		)
		.first<{ session_id: string }>();
	const gatewayId =
		typeof body.gateway_id === "string" && body.gateway_id
			? body.gateway_id
			: DEFAULT_GATEWAY_ID;
	const measurementId = newId();
	const commandId = newId();
	await db
		.prepare(
			"INSERT INTO commands (command_id,gateway_id,type,payload_json) VALUES (?,?,?,?)",
		)
		.bind(
			commandId,
			gatewayId,
			"arm_measurement",
			JSON.stringify({
				measurement_id: measurementId,
				session_id: current?.session_id || "",
				name,
			}),
		)
		.run();
	return new Response(
		JSON.stringify({ ok: true, measurement_id: measurementId }),
		{ headers: { "content-type": "application/json" } },
	);
};
