import type { APIRoute } from "astro";
export const GET: APIRoute = async () =>
	new Response(
		JSON.stringify({
			rover: [
				["Red blink", "No GNSS fix"],
				["Yellow", "Single / differential fix"],
				["Cyan", "RTK Float"],
				["Green", "RTK Fixed"],
				["Purple blink", "Gateway link lost"],
				["Blue", "Named measurement armed"],
				["White flash", "ESP-NOW communication"],
			],
			gateway: [
				["Red", "Rover unavailable"],
				["Orange", "Rover online, Internet unavailable"],
				["Blue", "Internet connected"],
				["Green", "Cloud API healthy"],
				["White flash", "Traffic"],
			],
		}),
		{ headers: { "content-type": "application/json" } },
	);
