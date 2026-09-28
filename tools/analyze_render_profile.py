"""Summarize real renderer CSVs using only the Python standard library."""
import argparse
import csv
import math
from pathlib import Path
import statistics


FIELDS = [
    "frame_interval_ms", "draw_total", "draw_world", "draw_overlay", "draw_post",
    "instanced_draws", "instances", "instance_upload_bytes", "texture_splits",
    "capacity_splits", "culled", "update_ms", "render_cpu_ms", "submit_cpu_ms",
    "post_cpu_ms", "swap_ms", "previous_log_ms", "font_hits", "font_misses",
    "font_upload_bytes", "legacy_draw_estimate", "legacy_vertices", "submitted_vertices",
]


def percentile(values, fraction):
    values = sorted(values)
    position = (len(values) - 1) * fraction
    lower = int(position)
    upper = min(lower + 1, len(values) - 1)
    return values[lower] + (values[upper] - values[lower]) * (position - lower)


def load(path, warmup, start, end):
    rows = []
    with path.open(encoding="utf-8-sig", newline="") as source:
        reader = csv.DictReader(source)
        required = set(FIELDS) | {"frame", "elapsed_s"}
        if not required.issubset(reader.fieldnames or []):
            raise ValueError(f"{path}: renderer CSV schema 2 is required")
        first_time = None
        for line, raw in enumerate(reader, 2):
            try:
                row = {key: float(raw[key]) for key in required}
            except (ValueError, TypeError) as error:
                raise ValueError(f"{path}:{line}: malformed or incomplete row") from error
            if not all(math.isfinite(value) and value >= 0 for value in row.values()):
                raise ValueError(f"{path}:{line}: invalid numeric value")
            if first_time is None:
                first_time = row["elapsed_s"]
            since_first_frame = row["elapsed_s"] - first_time
            if row["frame"] <= 1 or since_first_frame < max(warmup, start):
                continue
            if end is not None and since_first_frame > end:
                continue
            rows.append(row)
    if not rows or sum(row["frame_interval_ms"] for row in rows) <= 0:
        raise ValueError(f"{path}: no usable frames in the selected interval")
    return rows


def describe(path, rows):
    total_ms = sum(row["frame_interval_ms"] for row in rows)
    fps = len(rows) * 1000 / total_ms
    lines = [f"## {path.name}", "", f"- Source: `{path.resolve()}`",
             f"- Frames: {len(rows)}; accumulated frame intervals: {total_ms / 1000:.3f} s",
             f"- FPS (frames / elapsed frame intervals): {fps:.2f}", "",
             "| Metric | Mean | Median | P95 | P99 | Max |",
             "|---|---:|---:|---:|---:|---:|"]
    for field in FIELDS:
        values = [row[field] for row in rows]
        numbers = [statistics.mean(values), statistics.median(values),
                   percentile(values, .95), percentile(values, .99), max(values)]
        lines.append(f"| {field} | " + " | ".join(f"{number:.3f}" for number in numbers) + " |")
    geometry = sum(row["draw_world"] + row["draw_overlay"] for row in rows)
    legacy = sum(row["legacy_draw_estimate"] for row in rows)
    if legacy:
        lines += ["", f"Estimated geometry call reduction against adjacent-texture grouping: "
                  f"{100 * (1 - geometry / legacy):.2f}% (counterfactual, not an old-renderer timing)."]
    lines += ["", "CPU render includes submission and post-processing CPU time; do not sum them.",
              "No GPU duration is measured. Swap, scheduling, shader pixel cost and logging can affect FPS."]
    return lines, fps


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", type=Path)
    parser.add_argument("--baseline", type=Path, help="Another schema-2 CSV from comparable conditions")
    parser.add_argument("--warmup", type=float, default=5, help="Seconds from first frame to exclude")
    parser.add_argument("--start", type=float, default=0, help="Start seconds from first frame")
    parser.add_argument("--end", type=float, help="End seconds from first frame")
    parser.add_argument("--note", default="", help="Scene, movement path and settings used")
    parser.add_argument("--output", type=Path, help="Save Markdown; otherwise print to stdout")
    args = parser.parse_args()
    limits = [args.warmup, args.start] + ([] if args.end is None else [args.end])
    if any(not math.isfinite(value) or value < 0 for value in limits):
        parser.error("Time limits must be finite and nonnegative")
    if args.end is not None and args.end <= max(args.start, args.warmup):
        parser.error("End must be after both start and warmup")
    lines = ["# Renderer profile analysis", "", f"Notes: {args.note or 'Not supplied'}", "",
             f"Warmup: {args.warmup}s; start: {args.start}s; end: {args.end}", ""]
    try:
        rows = load(args.csv, args.warmup, args.start, args.end)
        report, fps = describe(args.csv, rows)
        lines.extend(report)
        if args.baseline:
            baseline = load(args.baseline, args.warmup, args.start, args.end)
            report, previous_fps = describe(args.baseline, baseline)
            lines += [""] + report
            lines += ["", f"FPS change: {100 * (fps / previous_fps - 1):+.2f}%.",
                      "This comparison is valid only with matching hardware, build, viewport, "
                      "post effects, scene, focus and input path; check the companion .log files."]
        text = "\n".join(lines) + "\n"
        if args.output:
            inputs = [args.csv.resolve()] + ([] if not args.baseline else [args.baseline.resolve()])
            if args.output.resolve() in inputs:
                parser.error("Output must not overwrite an input CSV")
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(text, encoding="utf-8")
        else:
            print(text, end="")
    except (OSError, ValueError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
