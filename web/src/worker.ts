import { handle } from "@astrojs/cloudflare/handler";
import { LiveStatus } from "./lib/live-status";

export { LiveStatus };

export default {
	async fetch(request, env, ctx) {
		return handle(request, env, ctx);
	},
} satisfies ExportedHandler<Env>;
