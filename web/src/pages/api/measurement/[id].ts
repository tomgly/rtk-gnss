import type { APIRoute } from "astro";
import { env } from "cloudflare:workers";
import { requireAdmin } from "../../../lib/auth";

export const PATCH: APIRoute = async (context) => {
  const denied = await requireAdmin(context); if (denied) return denied;
  const id = context.params.id; const body = (await context.request.json().catch(() => ({}))) as { name?: unknown };
  const name = String(body.name || "").trim();
  if (!id || !name) return new Response(JSON.stringify({ error: "id and name required" }), { status: 400 });
  await env.DB.prepare("UPDATE measurements SET name=?, updated_at=CURRENT_TIMESTAMP WHERE measurement_id=?").bind(name, id).run();
  return new Response(JSON.stringify({ ok: true }), { headers: { "content-type": "application/json" } });
};
