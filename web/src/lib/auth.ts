import type { APIContext } from "astro";

const COOKIE_NAME = "rtk_admin";
const SESSION_HOURS = 12;

export async function createAdminSession(context: APIContext) {
  const token = crypto.randomUUID().replaceAll("-", "");
  const expires = new Date(Date.now() + SESSION_HOURS * 3600_000).toISOString();
  await context.locals.runtime.env.DB.prepare(
    "INSERT INTO admin_sessions (token, expires_at) VALUES (?, ?)",
  )
    .bind(token, expires)
    .run();
  context.cookies.set(COOKIE_NAME, token, {
    httpOnly: true,
    secure: true,
    sameSite: "strict",
    path: "/",
    maxAge: SESSION_HOURS * 3600,
  });
}

export async function isAdmin(context: APIContext) {
  const token = context.cookies.get(COOKIE_NAME)?.value;
  if (!token) return false;
  const row = await context.locals.runtime.env.DB.prepare(
    "SELECT token FROM admin_sessions WHERE token = ? AND expires_at > strftime('%Y-%m-%dT%H:%M:%SZ','now')",
  )
    .bind(token)
    .first();
  return Boolean(row);
}

export async function requireAdmin(context: APIContext) {
  if (await isAdmin(context)) return null;
  return new Response(JSON.stringify({ error: "unauthorized" }), {
    status: 401,
    headers: { "content-type": "application/json" },
  });
}

export async function destroyAdminSession(context: APIContext) {
  const token = context.cookies.get(COOKIE_NAME)?.value;
  if (token) {
    await context.locals.runtime.env.DB.prepare("DELETE FROM admin_sessions WHERE token = ?")
      .bind(token)
      .run();
  }
  context.cookies.delete(COOKIE_NAME, { path: "/" });
}
