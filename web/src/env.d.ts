/// <reference types="astro/client" />

declare namespace Cloudflare {
	interface Env {
		DEVICE_API_TOKEN: string;
		LOGIN_PASSWORD: string;
	}
}
