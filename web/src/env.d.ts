/// <reference types="astro/client" />

type D1Database = import("@cloudflare/workers-types").D1Database;

interface Env {
  DB: D1Database;
  DEVICE_API_TOKEN: string;
  ADMIN_PASSWORD: string;
  DEFAULT_GATEWAY_ID: string;
}

declare namespace App {
  interface Locals {
    runtime: { env: Env };
  }
}
