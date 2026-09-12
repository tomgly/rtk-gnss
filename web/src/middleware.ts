import { defineMiddleware } from "astro:middleware";
import { isAuthenticated } from "./lib/auth";

export const onRequest = defineMiddleware(async (context, next) => {
	const { pathname } = context.url;
	const protectsPage =
		!pathname.startsWith("/api/") &&
		!pathname.startsWith("/_astro/") &&
		pathname !== "/login";
	const protectsApi = pathname.startsWith("/api/public/");

	if (!protectsPage && !protectsApi) return next();
	if (await isAuthenticated(context)) return next();

	if (protectsApi) {
		return new Response(JSON.stringify({ error: "unauthorized" }), {
			status: 401,
			headers: { "content-type": "application/json" },
		});
	}

	return context.redirect("/login");
});
