import type { APIRoute } from "astro";
import { env } from "cloudflare:workers";
import { requireAuthentication } from "../../../lib/auth";
import { DEFAULT_GATEWAY_ID } from "../../../lib/config";
import { newId } from "../../../lib/ids";

export const DELETE: APIRoute = async (context) => {
	const denied = await requireAuthentication(context);
	if (denied) return denied;
	const sessionId = context.params.id;
	if (!sessionId)
		return new Response(JSON.stringify({ error: "session id required" }), {
			status: 400,
		});

	const db = env.DB;
	const session = await db
		.prepare("SELECT active FROM sessions WHERE session_id=?")
		.bind(sessionId)
		.first<{ active: number }>();
	if (!session)
		return new Response(JSON.stringify({ error: "session not found" }), {
			status: 404,
		});

	if (session.active) {
		await db
			.prepare(
				"INSERT INTO commands (command_id,gateway_id,type,payload_json) VALUES (?,?,?,?)",
			)
			.bind(
				newId(),
				DEFAULT_GATEWAY_ID,
				"session_control",
				JSON.stringify({ session_id: sessionId, active: false }),
			)
			.run();
	}

	await db.batch([
		db.prepare("DELETE FROM telemetry WHERE session_id=?").bind(sessionId),
		db.prepare("DELETE FROM measurements WHERE session_id=?").bind(sessionId),
		db.prepare("DELETE FROM events WHERE session_id=?").bind(sessionId),
		db.prepare("DELETE FROM sessions WHERE session_id=?").bind(sessionId),
	]);

	return new Response(JSON.stringify({ ok: true }), {
		headers: { "content-type": "application/json" },
	});
};
