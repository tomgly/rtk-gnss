import { DurableObject } from "cloudflare:workers";

const STATUS_FRESH_MS = 15_000;
const D1_PERSIST_INTERVAL_MS = 60_000;

export type LiveStatusPayload = {
	type: "gateway_status" | "rover_status";
	record_id: string;
	device_id: string;
	gateway_id: string;
	gateway_time: string;
	espnow_rssi?: number;
	wifi_rssi?: number;
	protocol_version?: number;
};

type StoredStatus = {
	payload: LiveStatusPayload;
	receivedAt: number;
	persistedAt: number;
};

export function toLiveStatus(
	payload: Record<string, unknown>,
): LiveStatusPayload | null {
	const type = payload.type;
	const recordId = payload.record_id;
	const deviceId = payload.device_id;
	const gatewayId = payload.gateway_id;
	const gatewayTime = payload.gateway_time;
	if (
		(type !== "gateway_status" && type !== "rover_status") ||
		typeof recordId !== "string" ||
		typeof deviceId !== "string" ||
		typeof gatewayId !== "string" ||
		typeof gatewayTime !== "string"
	) {
		return null;
	}
	return {
		type,
		record_id: recordId,
		device_id: deviceId,
		gateway_id: gatewayId,
		gateway_time: gatewayTime,
		espnow_rssi:
			typeof payload.espnow_rssi === "number" ? payload.espnow_rssi : undefined,
		wifi_rssi:
			typeof payload.wifi_rssi === "number" ? payload.wifi_rssi : undefined,
		protocol_version:
			typeof payload.protocol_version === "number"
				? payload.protocol_version
				: undefined,
	};
}

export class LiveStatus extends DurableObject<Env> {
	async update(payload: LiveStatusPayload) {
		const key = `status:${payload.device_id}`;
		const current = await this.ctx.storage.get<StoredStatus>(key);
		const now = Date.now();
		const persist =
			!current ||
			now - current.receivedAt > STATUS_FRESH_MS ||
			now - current.persistedAt >= D1_PERSIST_INTERVAL_MS;
		await this.ctx.storage.put(key, {
			payload,
			receivedAt: now,
			persistedAt: persist ? now : current.persistedAt,
		} satisfies StoredStatus);
		return { persist };
	}

	async getStatus() {
		const statuses = await this.ctx.storage.list<StoredStatus>({
			prefix: "status:",
		});
		const current = [...statuses.values()].filter(
			(status) => Date.now() - status.receivedAt <= STATUS_FRESH_MS,
		);
		return {
			gateway:
				current.find((status) => status.payload.type === "gateway_status")
					?.payload ?? null,
			rover:
				current.find((status) => status.payload.type === "rover_status")
					?.payload ?? null,
		};
	}
}
