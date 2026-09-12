import type { APIRoute } from "astro";
export const GET: APIRoute = async (context) => {
  const rows = await context.locals.runtime.env.DB.prepare("SELECT * FROM sessions ORDER BY created_at DESC LIMIT 100").all();
  return new Response(JSON.stringify({ sessions: rows.results ?? [] }), { headers: { "content-type": "application/json" } });
};
