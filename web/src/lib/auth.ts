import type { APIContext } from "astro";
import { env } from "cloudflare:workers";

const COOKIE_NAME = "rtk_session";
const SESSION_HOURS = 12;
const encoder = new TextEncoder();

function toBase64Url(bytes: Uint8Array) {
	let binary = "";
	for (const byte of bytes) binary += String.fromCharCode(byte);
	return btoa(binary)
		.replaceAll("+", "-")
		.replaceAll("/", "_")
		.replaceAll("=", "");
}

function fromBase64Url(value: string) {
	const base64 = value
		.replaceAll("-", "+")
		.replaceAll("_", "/")
		.padEnd(Math.ceil(value.length / 4) * 4, "=");
	const binary = atob(base64);
	return Uint8Array.from(binary, (character) => character.charCodeAt(0));
}

async function getSigningKey() {
	return crypto.subtle.importKey(
		"raw",
		encoder.encode(env.LOGIN_PASSWORD),
		{ name: "HMAC", hash: "SHA-256" },
		false,
		["sign", "verify"],
	);
}

async function sign(value: string) {
	const signature = await crypto.subtle.sign(
		"HMAC",
		await getSigningKey(),
		encoder.encode(value),
	);
	return toBase64Url(new Uint8Array(signature));
}

export async function createSession(context: APIContext) {
	const expiresAt = Date.now() + SESSION_HOURS * 3600_000;
	const payload = `${expiresAt}.${crypto.randomUUID()}`;
	const signature = await sign(payload);
	context.cookies.set(COOKIE_NAME, `${payload}.${signature}`, {
		httpOnly: true,
		secure: true,
		sameSite: "strict",
		path: "/",
		maxAge: SESSION_HOURS * 3600,
	});
}

export async function isAuthenticated(context: APIContext) {
	const value = context.cookies.get(COOKIE_NAME)?.value;
	if (!value) return false;
	const [expiresAtText, token, signature, ...rest] = value.split(".");
	const expiresAt = Number(expiresAtText);
	if (
		!token ||
		!signature ||
		rest.length ||
		!Number.isSafeInteger(expiresAt) ||
		expiresAt <= Date.now()
	) {
		return false;
	}
	try {
		return crypto.subtle.verify(
			"HMAC",
			await getSigningKey(),
			fromBase64Url(signature),
			encoder.encode(`${expiresAtText}.${token}`),
		);
	} catch {
		return false;
	}
}

export async function requireAuthentication(context: APIContext) {
	if (await isAuthenticated(context)) return null;
	return new Response(JSON.stringify({ error: "unauthorized" }), {
		status: 401,
		headers: { "content-type": "application/json" },
	});
}

export async function destroySession(context: APIContext) {
	context.cookies.delete(COOKIE_NAME, { path: "/" });
}
