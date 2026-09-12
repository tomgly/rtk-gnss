import type { APIRoute } from "astro";
import { env } from "cloudflare:workers";
import { requireAuthentication } from "../../../lib/auth";
import { DEFAULT_GATEWAY_ID } from "../../../lib/config";
import { newId } from "../../../lib/ids";

export const POST: APIRoute = async (context) => {
	const denied = await requireAuthentication(context);
	if (denied) return denied;
	const body = (await context.request.json().catch(() => ({}))) as {
		action?: unknown;
		gateway_id?: unknown;
		name?: unknown;
	};
	const db = env.DB;
	const gatewayId =
		typeof body.gateway_id === "string" && body.gateway_id
			? body.gateway_id
			: DEFAULT_GATEWAY_ID;
	const action = body.action;
	if (action === "start") {
		const sessionId = newId();
		const commandId = newId();
		const name = String(
			body.name ||
				`Field Test ${new Date().toISOString().slice(0, 16).replace("T", " ")}`,
		);
		await db
			.prepare(
				"UPDATE sessions SET active=0, ended_at=COALESCE(ended_at, CURRENT_TIMESTAMP) WHERE active=1",
			)
			.run();
		await db
			.prepare(
				"INSERT INTO sessions (session_id,name,started_at,active,source) VALUES (?,?,CURRENT_TIMESTAMP,1,'web')",
			)
			.bind(sessionId, name)
			.run();
		await db
			.prepare(
				"INSERT INTO commands (command_id,gateway_id,type,payload_json) VALUES (?,?,?,?)",
			)
			.bind(
				commandId,
				gatewayId,
				"session_control",
				JSON.stringify({ session_id: sessionId, active: true }),
			)
			.run();
		return new Response(
			JSON.stringify({ ok: true, session_id: sessionId, name }),
			{
				headers: { "content-type": "application/json" },
			},
		);
	}
	if (action === "stop") {
		const current = await db
			.prepare(
				"SELECT session_id FROM sessions WHERE active=1 ORDER BY started_at DESC LIMIT 1",
			)
			.first<{ session_id: string }>();
		if (!current)
			return new Response(JSON.stringify({ error: "no active session" }), {
				status: 409,
			});
		const commandId = newId();
		await db
			.prepare(
				"UPDATE sessions SET active=0, ended_at=CURRENT_TIMESTAMP WHERE session_id=?",
			)
			.bind(current.session_id)
			.run();
		await db
			.prepare(
				"INSERT INTO commands (command_id,gateway_id,type,payload_json) VALUES (?,?,?,?)",
			)
			.bind(
				commandId,
				gatewayId,
				"session_control",
				JSON.stringify({ session_id: current.session_id, active: false }),
			)
			.run();
		return new Response(JSON.stringify({ ok: true }), {
			headers: { "content-type": "application/json" },
		});
	}
	return new Response(JSON.stringify({ error: "invalid action" }), {
		status: 400,
	});
};
