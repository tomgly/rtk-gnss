const fixLabel = (fix) =>
	fix === 4
		? "RTK FIXED"
		: fix === 5
			? "RTK FLOAT"
			: fix === 2
				? "DIFFERENTIAL"
				: fix === 1
					? "SINGLE"
					: "NO FIX";
const fixState = (fix) =>
	fix === 4
		? "fixed"
		: fix === 5
			? "float"
			: fix === 2
				? "differential"
				: fix === 1
					? "single"
					: "offline";
const txt = (id, value) => {
	const el = document.getElementById(id);
	if (el) el.textContent = value ?? "—";
};
const num = (v, digits) => (typeof v === "number" ? v.toFixed(digits) : "—");
const setState = (id, state) =>
	document.getElementById(id)?.setAttribute("data-state", state);
const updateSession = (session) => {
	const active = Boolean(session?.session_id);
	txt("session-name", session?.name || "No active session");
	txt("session-id", session?.session_id || "Start a session from controls");
	txt("session-state", active ? "Session active" : "No active session");
	const start = document.getElementById("start-session");
	const stop = document.getElementById("stop-session");
	if (start) {
		start.hidden = active;
		start.disabled = false;
	}
	txt("start-session-label", "Start");
	if (stop) {
		stop.hidden = !active;
		stop.disabled = false;
	}
	txt("stop-session-label", "Stop");
};
const isCurrentStatus = (status, maxAgeMs) => {
	const updatedAt = Date.parse(status?.gateway_time || "");
	return (
		Number.isFinite(updatedAt) &&
		updatedAt <= Date.now() &&
		Date.now() - updatedAt <= maxAgeMs
	);
};
const isCurrentDeviceStatus = (status) => isCurrentStatus(status, 15000);
const isCurrentTelemetry = (status) => isCurrentStatus(status, 120000);
const hasCurrentGnssData = (status) =>
	isCurrentTelemetry(status) &&
	Number(status?.fix) > 0 &&
	Number(status?.gnss_age_ms) <= 5000;
const updateOnlineState = (gateway, rover, status, connection) => {
	const gatewayOnline =
		connection?.gateway_online ??
		isCurrentDeviceStatus({ gateway_time: gateway?.last_seen });
	const roverOnline =
		connection?.rover_online ??
		isCurrentDeviceStatus({ gateway_time: rover?.last_seen });
	const telemetryCurrent = isCurrentTelemetry(status);
	const roverState = roverOnline
		? telemetryCurrent
			? "connected"
			: "no-data"
		: "offline";
	txt(
		"gateway-state-label",
		gatewayOnline ? "Gateway Online" : "Gateway Offline",
	);
	txt(
		"rover-state-label",
		roverOnline
			? telemetryCurrent
				? "Rover Online"
				: "Rover Online · No Data"
			: "Rover Offline",
	);
	setState("gateway-state", gatewayOnline ? "connected" : "offline");
	setState("rover-state", roverState);
	return {
		gatewayOnline,
		roverOnline,
		rover,
		telemetry: telemetryCurrent ? status : {},
	};
};
async function refresh() {
	try {
		const response = await fetch("/api/public/live", { cache: "no-store" });
		if (!response.ok) throw new Error("Live request failed");
		const data = await response.json();
		const state = updateOnlineState(
			data.gateway,
			data.rover,
			data.telemetry,
			data.connection,
		);
		const t = state.telemetry;
		const noData = state.roverOnline && !isCurrentTelemetry(data.telemetry);
		const hasPosition = hasCurrentGnssData(data.telemetry);
		txt("fix", noData ? "No Data" : fixLabel(t.fix));
		setState("gnss-status", noData ? "no-data" : fixState(t.fix));
		txt("sats", t.satellites);
		txt("hdop", t.hdop);
		txt("esp-rssi", t.espnow_rssi ?? state.rover?.last_espnow_rssi);
		txt("lost", t.packets_lost);
		txt("wifi-rssi", state.gatewayOnline ? data.gateway?.last_wifi_rssi : "—");
		txt("wifi-ssid", state.gatewayOnline ? "Online" : "Offline");
		txt("lat", hasPosition ? num(t.lat, 7) : "—");
		txt("lon", hasPosition ? num(t.lon, 7) : "—");
		txt("alt", hasPosition ? num(t.alt_m, 2) : "—");
		updateSession(data.activeSession);
	} catch {
		updateOnlineState(null, null, null);
	}
}
refresh();
setInterval(refresh, 5000);

const controls = document.getElementById("controls");
const controlsSummary = controls?.querySelector("summary");
const desktop = window.matchMedia("(min-width: 721px)");
const updateControls = () => {
	if (!controls) return;
	controls.open = desktop.matches;
	if (controlsSummary) controlsSummary.tabIndex = desktop.matches ? -1 : 0;
};
updateControls();
desktop.addEventListener("change", updateControls);
controlsSummary?.addEventListener("click", (event) => {
	if (desktop.matches) event.preventDefault();
});

if (document.documentElement.dataset.authenticated === "true") {
	document.getElementById("arm-form")?.addEventListener("submit", async (e) => {
		e.preventDefault();
		const input = document.getElementById("measurement-name");
		const name = input?.value.trim();
		if (!name) return;
		const r = await fetch("/api/measurement/arm", {
			method: "POST",
			headers: { "content-type": "application/json" },
			body: JSON.stringify({ name }),
		});
		if (r.ok) {
			txt("arm-state", `Armed: ${name}`);
			input.value = "";
		}
	});
	document
		.getElementById("start-session")
		?.addEventListener("click", async () => {
			const button = document.getElementById("start-session");
			if (button?.disabled) return;
			const name = document.getElementById("session-name-input")?.value.trim();
			button.disabled = true;
			txt("start-session-label", "Starting…");
			txt("session-state", "Starting session…");
			const r = await fetch("/api/session", {
				method: "POST",
				headers: { "content-type": "application/json" },
				body: JSON.stringify({ action: "start", name }),
			});
			if (!r.ok) {
				button.disabled = false;
				txt("start-session-label", "Start");
				txt("session-state", "Could not start session");
				return;
			}
			const session = await r.json();
			updateSession(session);
			document.getElementById("session-name-input").value = "";
		});
	document
		.getElementById("stop-session")
		?.addEventListener("click", async () => {
			const button = document.getElementById("stop-session");
			if (button?.disabled) return;
			button.disabled = true;
			txt("stop-session-label", "Stopping…");
			txt("session-state", "Stopping session…");
			const r = await fetch("/api/session", {
				method: "POST",
				headers: { "content-type": "application/json" },
				body: JSON.stringify({ action: "stop" }),
			});
			if (!r.ok) {
				button.disabled = false;
				txt("stop-session-label", "Stop");
				txt("session-state", "Could not stop session");
				return;
			}
			updateSession(null);
		});
}
