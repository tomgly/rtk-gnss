import type { APIRoute } from "astro";
import { env } from "cloudflare:workers";

export const GET: APIRoute = async () => {
	const db = env.DB;
	const telemetry = await db
		.prepare(
			"SELECT * FROM telemetry WHERE record_id LIKE 'live:%' ORDER BY gateway_time DESC LIMIT 1",
		)
		.first();
	const persistedGateway = await db
		.prepare(
			"SELECT * FROM devices WHERE device_id = gateway_id ORDER BY last_seen DESC LIMIT 1",
		)
		.first();
	const persistedRover = await db
		.prepare(
			"SELECT * FROM devices WHERE device_id != gateway_id ORDER BY last_seen DESC LIMIT 1",
		)
		.first();
	const liveStatus = persistedGateway
		? await env.LIVE_STATUS.getByName(
				String(persistedGateway.gateway_id ?? persistedGateway.device_id),
			).getStatus()
		: null;
	const gateway = liveStatus?.gateway
		? liveStatus.gateway
		: liveStatus
			? null
			: persistedGateway;
	const rover = liveStatus?.rover
		? liveStatus.rover
		: liveStatus
			? null
			: persistedRover;
	const measurement = await db
		.prepare("SELECT * FROM measurements ORDER BY gateway_time DESC LIMIT 1")
		.first();
	const activeSession = await db
		.prepare(
			"SELECT * FROM sessions WHERE active=1 ORDER BY started_at DESC LIMIT 1",
		)
		.first();
	return new Response(
		JSON.stringify({
			gateway,
			rover,
			telemetry,
			measurement,
			activeSession,
			connection: {
				gateway_online: Boolean(liveStatus?.gateway),
				rover_online: Boolean(liveStatus?.rover),
			},
		}),
		{
			headers: {
				"content-type": "application/json",
				"cache-control": "no-store",
			},
		},
	);
};
