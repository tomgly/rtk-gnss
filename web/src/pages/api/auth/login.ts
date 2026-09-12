import type { APIRoute } from "astro";
import { env } from "cloudflare:workers";
import { createAdminSession } from "../../../lib/auth";

export const POST: APIRoute = async (context) => {
  const body = (await context.request.json().catch(() => ({}))) as { password?: unknown };
  if (body.password !== env.ADMIN_PASSWORD) {
    return new Response(JSON.stringify({ error: "invalid_credentials" }), { status: 401 });
  }
  await createAdminSession(context);
  return new Response(JSON.stringify({ ok: true }), { headers: { "content-type": "application/json" } });
};
