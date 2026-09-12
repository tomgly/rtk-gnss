/// <reference types="astro/client" />

declare namespace Cloudflare {
  interface Env {
    DEVICE_API_TOKEN: string;
    ADMIN_PASSWORD: string;
  }
}
