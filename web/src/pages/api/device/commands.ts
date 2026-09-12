import type { APIRoute } from "astro";
import { requireDevice } from "../../../lib/device-auth";

export const GET: APIRoute = async (context) => {
  const denied = requireDevice(context);
  if (denied) return denied;
  const deviceId = context.url.searchParams.get("device_id");
  const after = Number(context.url.searchParams.get("after") ?? "0");
  if (!deviceId) return new Response(JSON.stringify({ error: "device_id required" }), { status: 400 });
  const rows = await context.locals.runtime.env.DB.prepare(
    "SELECT id, command_id, type, payload_json FROM commands WHERE gateway_id=? AND id>? ORDER BY id ASC LIMIT 20",
  ).bind(deviceId, after).all();
  const commands = (rows.results ?? []).map((row: any) => ({ id: row.id, command_id: row.command_id, type: row.type, ...JSON.parse(row.payload_json) }));
  return new Response(JSON.stringify({ commands }), { headers: { "content-type": "application/json" } });
};
