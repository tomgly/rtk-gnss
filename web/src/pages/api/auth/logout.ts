import type { APIRoute } from "astro";
import { destroyAdminSession } from "../../../lib/auth";

export const POST: APIRoute = async (context) => {
  await destroyAdminSession(context);
  return new Response(JSON.stringify({ ok: true }), { headers: { "content-type": "application/json" } });
};
