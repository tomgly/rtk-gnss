import type { APIRoute } from "astro";
export const GET: APIRoute = async (context) => {
  const rows = await context.locals.runtime.env.DB.prepare("SELECT * FROM measurements ORDER BY gateway_time DESC LIMIT 200").all();
  return new Response(JSON.stringify({ measurements: rows.results ?? [] }), { headers: { "content-type": "application/json" } });
};
