"""Loopback-only JSON receiver and HTML dashboard. Python 3.9+, standard library only."""
import argparse
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import math
from pathlib import Path
import re
import threading
import time
import os
import webbrowser

STATIC = Path(__file__).resolve().parent / "monitor"
MAX_BODY = 2 * 1024 * 1024
MAX_SAFE = 2**53 - 1


def number(obj, key, low=0, high=MAX_SAFE, integer=False):
    value = obj.get(key)
    if type(value) not in (int, float):
        raise ValueError(f"{key}: finite number required")
    if not low <= value <= high or not math.isfinite(value) or (integer and type(value) is not int):
        raise ValueError(f"{key}: invalid range/type")
    return value


def text(obj, key, limit=512):
    value = obj.get(key)
    if not isinstance(value, str) or not value or len(value) > limit:
        raise ValueError(f"{key}: nonempty string required")
    return value


def boolean(obj, key):
    if type(obj.get(key)) is not bool:
        raise ValueError(f"{key}: boolean required")


def object_field(obj, key):
    value = obj.get(key)
    if not isinstance(value, dict):
        raise ValueError(f"{key}: object required")
    return value


def coordinate(obj, key):
    value = text(obj, key, 21)
    if not re.fullmatch(r"-?(0|[1-9][0-9]*)", value) or not -(2**63) <= int(value) < 2**63:
        raise ValueError(f"{key}: int64 decimal string required")


def validate_snapshot(data):
    if not isinstance(data, dict) or type(data.get("schema_version")) is not int or data.get("schema_version") != 1:
        raise ValueError("schema_version 1 required")
    if data.get("type") != "world_snapshot" or data.get("scope") != "loaded_chunks":
        raise ValueError("world_snapshot / loaded_chunks required")
    text(data, "session_id", 128)
    number(data, "sequence", 1, integer=True)
    number(data, "reset_epoch", integer=True)
    number(data, "interval_ms", 250, 60000, integer=True)
    number(data, "uptime_seconds")
    world = object_field(data, "world")
    for key in ("origin_chunk_x", "origin_chunk_y"):
        coordinate(world, key)
    for key in ("loaded_chunks", "actors", "remembered_npcs"):
        number(world, key, integer=True)
    simulation = object_field(data, "simulation")
    boolean(simulation, "focused")
    boolean(simulation, "paused")
    if not isinstance(simulation.get("reason"), str) or len(simulation["reason"]) > 256:
        raise ValueError("simulation reason required")
    player = object_field(data, "player")
    number(player, "level", 1, integer=True)
    number(player, "max_health", 1, integer=True)
    number(player, "health", 0, player["max_health"], integer=True)
    number(player, "legitimacy", -100, 100, integer=True)
    number(player, "reputation", -MAX_SAFE, integer=True)
    number(player, "story_stage", 0, 7, integer=True)
    text(player, "objective", 2048)
    profiling = object_field(data, "profiling")
    if not re.fullmatch(r"[0-9]{1,20}", text(profiling, "frame", 20)):
        raise ValueError("profiling frame required")
    for key in ("fps", "interval_ms", "update_cpu_ms", "render_cpu_ms", "submit_cpu_ms", "post_cpu_ms", "swap_ms"):
        number(profiling, key)
    for key in ("draw_total", "draw_world", "draw_overlay", "draw_post", "instances", "upload_bytes"):
        number(profiling, key, integer=True)
    if profiling["draw_total"] != sum(profiling[key] for key in ("draw_world", "draw_overlay", "draw_post")):
        raise ValueError("draw total mismatch")
    for key in ("sent", "failed", "replaced"):
        number(object_field(data, "transport"), key, integer=True)

    for key in ("villages", "npcs", "mobs"):
        if not isinstance(data.get(key), list) or len(data[key]) > 10000:
            raise ValueError(f"{key}: bounded array required")
    villages = {}
    for village in data["villages"]:
        if not isinstance(village, dict):
            raise ValueError("village must be an object")
        identity = text(village, "id")
        if identity in villages:
            raise ValueError("duplicate village ID")
        text(village, "name")
        number(village, "expected_chunks", 1, 10000, integer=True)
        number(village, "loaded_chunks", 0, village["expected_chunks"], integer=True)
        if village.get("kind") == "town":
            for field in ("security", "target_population", "initial_hostiles", "population", "living_population", "hostile_population", "travelling_population"):
                number(village, field, 0, 10000, integer=True)
            number(village, "security", 0, 100, integer=True)
        villages[identity] = village
    if sum(v["loaded_chunks"] for v in villages.values()) != world["loaded_chunks"]:
        raise ValueError("loaded chunk total mismatch")

    def position(entity):
        if entity.get("village_id") not in villages:
            raise ValueError("unknown village_id")
        point = object_field(entity, "position")
        coordinate(point, "chunk_x")
        coordinate(point, "chunk_y")
        number(point, "local_x", 0, 8)
        number(point, "local_y", 0, 8)

    position(player)
    identities = set()
    for key in ("npcs", "mobs"):
        for entity in data[key]:
            if not isinstance(entity, dict):
                raise ValueError("entity must be an object")
            identity = text(entity, "id")
            if identity in identities:
                raise ValueError("duplicate Actor ID")
            identities.add(identity)
            text(entity, "name")
            number(entity, "max_health", 1, integer=True)
            number(entity, "health", 0, entity["max_health"], integer=True)
            boolean(entity, "enabled")
            position(entity)
            if key == "npcs":
                number(entity, "job_id", 0, 6, integer=True)
                number(entity, "activity_id", 0, 10, integer=True)
                text(entity, "job")
                text(entity, "activity")
                number(entity, "notoriety", 0, 100, integer=True)
                number(entity, "trust", -100, 100, integer=True)
                for flag in ("loaded", "travelling"):
                    if flag in entity:
                        boolean(entity, flag)
                if "home_town" in entity and entity["home_town"] not in villages:
                    raise ValueError("unknown home_town")
                if entity.get("destination_town") and entity["destination_town"] not in villages:
                    raise ValueError("unknown destination_town")
                for count in ("crimes", "services"):
                    number(entity, count, integer=True)
                for flag in ("helped", "apology", "looted", "active", "hostile", "story"):
                    boolean(entity, flag)
            else:
                text(entity, "state")
                for flag in ("combat_enabled", "tutorial", "loot"):
                    boolean(entity, flag)
                for value in ("windup", "cooldown", "respawn"):
                    number(entity, value)
    # Reject escaped unpaired surrogates and non-finite numbers in extension fields too.
    json.dumps(data, ensure_ascii=False, allow_nan=False).encode("utf-8")
    return data


def summarize(data):
    """NPCs and mobs are separate Actor sets; hostile citizens remain NPCs."""
    groups = {}
    for village in data["villages"]:
        groups[village["id"]] = {
            **village, "npc_loaded": 0, "npc_active": 0, "npc_living": 0,
            "npc_hostile": 0, "npc_incapacitated": 0, "mob_loaded": 0,
            "mob_living": 0, "mob_combat": 0, "mob_defeated": 0,
            "jobs": {}, "activities": {}, "notoriety_sum": 0, "trust_sum": 0,
            "health_sum": 0, "extorting": 0, "high_notoriety": 0,
        }
    for npc in data["npcs"]:
        group = groups[npc["village_id"]]
        if not npc.get("loaded", True):
            continue
        group["npc_loaded"] += 1
        if not (npc["active"] and npc["enabled"]):
            continue
        group["npc_active"] += 1
        group["npc_incapacitated"] += npc["health"] <= 0
        if npc["health"] <= 0:
            continue
        group["npc_living"] += 1
        group["npc_hostile"] += npc["hostile"]
        group["notoriety_sum"] += npc["notoriety"]
        group["trust_sum"] += npc["trust"]
        group["health_sum"] += npc["health"] / npc["max_health"] * 100
        group["extorting"] += npc["activity_id"] == 5
        group["high_notoriety"] += npc["notoriety"] >= 60
        for field, label in (("jobs", "job"), ("activities", "activity")):
            group[field][npc[label]] = group[field].get(npc[label], 0) + 1
    for mob in data["mobs"]:
        group = groups[mob["village_id"]]
        group["mob_loaded"] += 1
        group["mob_living"] += mob["enabled"] and mob["health"] > 0
        group["mob_combat"] += mob["combat_enabled"] and mob["health"] > 0
        group["mob_defeated"] += mob["enabled"] and mob["health"] <= 0
    for group in groups.values():
        living = group["npc_living"]
        for metric in ("notoriety", "trust", "health"):
            group[f"avg_{metric}"] = round(group.pop(f"{metric}_sum") / living, 1) if living else None
        group["status"] = (
            "위험" if group["npc_hostile"] or group["mob_combat"] or group["extorting"] or group.get("hostile_population", 0) else
            "불안" if group["high_notoriety"] else
            "안정" if living or group.get("living_population", 0) else "인구 미관측"
        )
        group["partial"] = group["loaded_chunks"] < group["expected_chunks"]
    keys = ("npc_loaded", "npc_active", "npc_living", "npc_hostile", "npc_incapacitated",
            "mob_loaded", "mob_living", "mob_combat", "mob_defeated")
    return {"counts": {key: sum(g[key] for g in groups.values()) for key in keys},
            "villages": list(groups.values())}


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def reject_constant(value):
    raise ValueError(f"invalid JSON constant: {value}")


class MonitorState:
    def __init__(self, record_dir=None):
        self.lock = threading.Lock()
        self.latest = None
        self.received = 0.0
        self.received_at = None
        self.record_dir = record_dir
        self.prefix = datetime.now(timezone.utc).strftime("world-%Y%m%dT%H%M%S-%f") + f"-{os.getpid()}"
        self.part = 0
        self.file = None
        self.record_bytes = 0
        self.record_error = None
        if record_dir:
            record_dir.mkdir(parents=True, exist_ok=True)

    def accept(self, snapshot):
        summary = summarize(snapshot)
        with self.lock:
            if self.latest and snapshot["session_id"] == self.latest["snapshot"]["session_id"]:
                if snapshot["sequence"] <= self.latest["snapshot"]["sequence"]:
                    return False
            self.received = time.monotonic()
            self.received_at = datetime.now(timezone.utc).isoformat()
            self.latest = {"snapshot": snapshot, "summary": summary}
            self.record({"received_at": self.received_at, **self.latest})
            return True

    def record(self, row):
        if not self.record_dir or self.record_error:
            return
        try:
            payload = (json.dumps(row, ensure_ascii=False, allow_nan=False, separators=(",", ":")) + "\n").encode("utf-8")
            if self.file is None or self.record_bytes + len(payload) > 16 * 1024 * 1024:
                if self.file:
                    self.file.close()
                self.part += 1
                self.file = (self.record_dir / f"{self.prefix}-{self.part:04d}.jsonl").open("xb")
                self.record_bytes = 0
                print(f"[Monitor] JSONL: {self.file.name}", flush=True)
            self.file.write(payload)
            self.file.flush()
            self.record_bytes += len(payload)
        except OSError as error:
            self.record_error = str(error)
            if self.file:
                try:
                    self.file.close()
                except OSError:
                    pass
                self.file = None
            print(f"[Monitor] Recording disabled: {error}", flush=True)

    def read(self):
        with self.lock:
            age = time.monotonic() - self.received if self.latest else None
            stale = not self.latest or age > max(5, self.latest["snapshot"]["interval_ms"] / 1000 * 3)
            return {"connected": bool(self.latest) and not stale, "stale": stale,
                    "age_seconds": round(age, 2) if age is not None else None,
                    "received_at": self.received_at, "record_error": self.record_error,
                    "recording_enabled": bool(self.record_dir) and not self.record_error,
                    "record_path": str(self.record_dir.resolve()) if self.record_dir else None,
                    **(self.latest or {"snapshot": None, "summary": None})}

    def close(self):
        with self.lock:
            if self.file:
                self.file.close()
                self.file = None


class Receiver(BaseHTTPRequestHandler):
    def setup(self):
        super().setup()
        self.connection.settimeout(2)

    def log_message(self, *_args):
        pass  # Avoid per-second console spam; JSONL contains the accepted messages.

    def send(self, status, body=b"", content_type="application/json; charset=utf-8"):
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Content-Security-Policy", "default-src 'self'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'")
        self.end_headers()
        if body:
            self.wfile.write(body)

    def send_json(self, status, data):
        self.send(status, json.dumps(data, ensure_ascii=False, allow_nan=False).encode("utf-8"))

    def allowed(self):
        port = self.server.server_port
        hosts = {f"127.0.0.1:{port}", f"localhost:{port}"}
        origin = self.headers.get("Origin")
        return self.headers.get("Host") in hosts and (not origin or origin in {f"http://{host}" for host in hosts})

    def do_GET(self):
        if not self.allowed():
            self.send_json(403, {"error": "local origin required"})
            return
        path = self.path.split("?", 1)[0]
        if path == "/api/snapshot":
            self.send_json(200, self.server.state.read())
        elif path == "/api/health":
            self.send_json(200, {"status": "ok", "schema_version": 1})
        elif path in ("/", "/index.html", "/app.js", "/style.css"):
            filename = "index.html" if path == "/" else path[1:]
            types = {"index.html": "text/html; charset=utf-8", "app.js": "text/javascript; charset=utf-8", "style.css": "text/css; charset=utf-8"}
            self.send(200, (STATIC / filename).read_bytes(), types[filename])
        else:
            self.send_json(404, {"error": "not found"})

    def do_POST(self):
        if not self.allowed():
            self.send_json(403, {"error": "local origin required"})
            return
        if self.path != "/api/snapshot":
            self.send_json(404, {"error": "not found"})
            return
        if self.headers.get_content_type() != "application/json":
            self.send_json(415, {"error": "application/json required"})
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if not 0 < length <= MAX_BODY or self.headers.get("Transfer-Encoding"):
                self.send_json(413, {"error": "body must be 1..2097152 bytes with Content-Length"})
                return
            raw = self.rfile.read(length)
            if len(raw) != length:
                raise ValueError("incomplete body")
            data = json.loads(raw.decode("utf-8"), object_pairs_hook=unique_object, parse_constant=reject_constant)
            validate_snapshot(data)
            if not self.server.state.accept(data):
                self.send_json(409, {"error": "out-of-order sequence"})
                return
            self.send(204)
        except (ValueError, TypeError, OverflowError, RecursionError) as error:
            self.send_json(400, {"error": str(error)[:256]})
        except (TimeoutError, ConnectionError):
            self.close_connection = True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--open-browser", action="store_true")
    parser.add_argument("--no-record", action="store_true")
    parser.add_argument("--record-dir", type=Path, default=STATIC / "Logs")
    args = parser.parse_args()
    if not 1024 <= args.port <= 65535:
        parser.error("port must be 1024..65535")
    state = MonitorState(None if args.no_record else args.record_dir)
    try:
        server = ThreadingHTTPServer(("127.0.0.1", args.port), Receiver)
    except OSError as error:
        parser.exit(1, f"Cannot listen on 127.0.0.1:{args.port}: {error}\n")
    server.state = state
    url = f"http://127.0.0.1:{args.port}/"
    print(f"[Monitor] Dashboard: {url}\n[Monitor] POST JSON: {url}api/snapshot\n[Monitor] Stop: Ctrl+C", flush=True)
    if args.open_browser:
        webbrowser.open(url)
    try:
        server.serve_forever(poll_interval=0.25)
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
        state.close()


if __name__ == "__main__":
    main()
