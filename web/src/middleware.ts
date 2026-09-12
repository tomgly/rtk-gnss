import { defineMiddleware } from "astro:middleware";
import { isAuthenticated } from "./lib/auth";

export const onRequest = defineMiddleware(async (context, next) => {
	const { pathname } = context.url;
	const protectsPage =
		!pathname.startsWith("/api/") &&
		!pathname.startsWith("/_astro/") &&
		pathname !== "/login";

	if (!protectsPage) return next();
	if (await isAuthenticated(context)) return next();

	return context.redirect("/login");
});
