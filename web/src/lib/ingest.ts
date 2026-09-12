type Payload = Record<string, unknown>;

const n = (v: unknown) => (typeof v === "number" ? v : null);
const s = (v: unknown) => (typeof v === "string" ? v : null);
const b = (v: unknown) => (v ? 1 : 0);

export async function ingest(db: D1Database, p: Payload) {
	const type = s(p.type);
	const recordId = s(p.record_id);
	const deviceId = s(p.device_id);
	const gatewayId = s(p.gateway_id);
	const gatewayTime = s(p.gateway_time);
	if (!type || !recordId || !deviceId || !gatewayId || !gatewayTime) {
		throw new Error("missing required envelope fields");
	}

	await db
		.prepare(
			`INSERT INTO devices (device_id, gateway_id, last_seen, last_espnow_rssi, last_wifi_rssi, protocol_version)
       VALUES (?, ?, ?, ?, ?, ?)
       ON CONFLICT(device_id) DO UPDATE SET gateway_id=excluded.gateway_id,last_seen=excluded.last_seen,
       last_espnow_rssi=excluded.last_espnow_rssi,last_wifi_rssi=excluded.last_wifi_rssi,
       protocol_version=excluded.protocol_version`,
		)
		.bind(
			deviceId,
			gatewayId,
			gatewayTime,
			n(p.espnow_rssi),
			n(p.wifi_rssi),
			n(p.protocol_version),
		)
		.run();

	const raw = JSON.stringify(p);
	if (type === "telemetry") {
		const liveRecordId = `live:${gatewayId}`;
		await db
			.prepare(
				`INSERT INTO telemetry
		(record_id,device_id,gateway_id,session_id,gateway_time,gnss_time,lat,lon,alt_m,fix,satellites,hdop,
         speed_mps,course_deg,gnss_age_ms,espnow_rssi,packets_received,packets_lost,wifi_ssid,wifi_rssi,
		   ntrip_enabled,ntrip_connected,rtcm_age_ms,rtcm_bytes,protocol_version,raw_json)
         VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
         ON CONFLICT(record_id) DO UPDATE SET
		   device_id=excluded.device_id,gateway_id=excluded.gateway_id,session_id=excluded.session_id,gateway_time=excluded.gateway_time,
           gnss_time=excluded.gnss_time,lat=excluded.lat,lon=excluded.lon,alt_m=excluded.alt_m,fix=excluded.fix,
           satellites=excluded.satellites,hdop=excluded.hdop,speed_mps=excluded.speed_mps,
           course_deg=excluded.course_deg,gnss_age_ms=excluded.gnss_age_ms,espnow_rssi=excluded.espnow_rssi,
           packets_received=excluded.packets_received,packets_lost=excluded.packets_lost,wifi_ssid=excluded.wifi_ssid,
           wifi_rssi=excluded.wifi_rssi,ntrip_enabled=excluded.ntrip_enabled,ntrip_connected=excluded.ntrip_connected,
           rtcm_age_ms=excluded.rtcm_age_ms,rtcm_bytes=excluded.rtcm_bytes,protocol_version=excluded.protocol_version,
           raw_json=excluded.raw_json`,
			)
			.bind(
				liveRecordId,
				deviceId,
				gatewayId,
				s(p.session_id),
				gatewayTime,
				s(p.gnss_time),
				n(p.lat),
				n(p.lon),
				n(p.alt_m),
				n(p.fix),
				n(p.satellites),
				n(p.hdop),
				n(p.speed_mps),
				n(p.course_deg),
				n(p.gnss_age_ms),
				n(p.espnow_rssi),
				n(p.packets_received),
				n(p.packets_lost),
				s(p.wifi_ssid),
				n(p.wifi_rssi),
				b(p.ntrip_enabled),
				b(p.ntrip_connected),
				n(p.rtcm_age_ms),
				n(p.rtcm_bytes),
				n(p.protocol_version),
				raw,
			)
			.run();
	} else if (type === "measurement") {
		const measurementId = s(p.measurement_id) ?? recordId;
		await db
			.prepare(
				`INSERT OR IGNORE INTO measurements
        (measurement_id,record_id,device_id,gateway_id,session_id,name,trigger,gateway_time,gnss_time,lat,lon,
         alt_m,fix,satellites,hdop,espnow_rssi,raw_json)
         VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)`,
			)
			.bind(
				measurementId,
				recordId,
				deviceId,
				gatewayId,
				s(p.session_id),
				s(p.name) ?? "Unnamed point",
				n(p.trigger),
				gatewayTime,
				s(p.gnss_time),
				n(p.lat),
				n(p.lon),
				n(p.alt_m),
				n(p.fix),
				n(p.satellites),
				n(p.hdop),
				n(p.espnow_rssi),
				raw,
			)
			.run();
	} else if (type === "event") {
		const eventId = s(p.event_id) ?? recordId;
		await db
			.prepare(
				`INSERT OR IGNORE INTO events
        (event_id,record_id,device_id,gateway_id,session_id,event_type,detail,gateway_time,espnow_rssi,raw_json)
        VALUES (?,?,?,?,?,?,?,?,?,?)`,
			)
			.bind(
				eventId,
				recordId,
				deviceId,
				gatewayId,
				s(p.session_id),
				s(p.event_type) ?? "event",
				s(p.detail),
				gatewayTime,
				n(p.espnow_rssi),
				raw,
			)
			.run();
		if (p.event_type === "measurement_cancel" && s(p.detail)) {
			await db
				.prepare(
					"UPDATE measurements SET cancelled=1, updated_at=CURRENT_TIMESTAMP WHERE measurement_id=?",
				)
				.bind(s(p.detail))
				.run();
		}
	} else if (type === "session") {
		const sessionId = s(p.session_id);
		if (!sessionId) throw new Error("session_id required");
		const active = b(p.active);
		await db
			.prepare(
				`INSERT INTO sessions (session_id,name,started_at,ended_at,active,source)
         VALUES (?, ?, CASE WHEN ?=1 THEN ? ELSE NULL END, CASE WHEN ?=0 THEN ? ELSE NULL END, ?, 'device')
         ON CONFLICT(session_id) DO UPDATE SET
           active=excluded.active,
           started_at=COALESCE(sessions.started_at, excluded.started_at),
           ended_at=CASE WHEN excluded.active=0 THEN excluded.ended_at ELSE sessions.ended_at END`,
			)
			.bind(
				sessionId,
				`Session ${sessionId.slice(0, 6)}`,
				active,
				gatewayTime,
				active,
				gatewayTime,
				active,
			)
			.run();
	} else {
		throw new Error(`unsupported type: ${type}`);
	}

	return { accepted: recordId };
}
