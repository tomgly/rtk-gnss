import type { APIRoute } from "astro";
import { destroySession } from "../../../lib/auth";

export const POST: APIRoute = async (context) => {
	await destroySession(context);
	return context.redirect("/login", 303);
};
