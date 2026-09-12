import type { APIRoute } from "astro";

export const GET: APIRoute = async (context) => {
  const db = context.locals.runtime.env.DB;
  const telemetry = await db.prepare("SELECT * FROM telemetry ORDER BY gateway_time DESC LIMIT 1").first();
  const measurement = await db.prepare("SELECT * FROM measurements ORDER BY gateway_time DESC LIMIT 1").first();
  const activeSession = await db.prepare("SELECT * FROM sessions WHERE active=1 ORDER BY started_at DESC LIMIT 1").first();
  return new Response(JSON.stringify({ telemetry, measurement, activeSession }), {
    headers: { "content-type": "application/json", "cache-control": "no-store" },
  });
};
