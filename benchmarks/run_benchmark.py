import argparse
import json
import os
import platform
import re
import shutil
import statistics
import subprocess
import sys
import time
import uuid
from datetime import datetime, timezone
from pathlib import Path

from benchmarks.generate_dataset import MAX_VIDEO_COUNT, generate_dataset, validate_dataset


ROOT = Path(__file__).resolve().parents[1]
WINDOW_PATTERN = re.compile(r'^\s*(0x[0-9a-fA-F]+)\s+"([^"]+)"')


def _reject_symlinked_path(path: Path, purpose: str) -> Path:
    absolute_path = Path(os.path.abspath(path))
    current = Path(absolute_path.anchor)
    for component in absolute_path.parts[1:]:
        current /= component
        if current.is_symlink():
            raise ValueError(f"refusing symlinked {purpose} path component: {current}")
    return absolute_path


def create_isolated_environment(runtime_directory: Path) -> dict[str, str]:
    runtime_directory.mkdir(parents=True, exist_ok=False)
    environment = os.environ.copy()
    for key, name in (
        ("XDG_DATA_HOME", "data"),
        ("XDG_CONFIG_HOME", "config"),
        ("XDG_CACHE_HOME", "cache"),
    ):
        path = runtime_directory / name
        path.mkdir()
        environment[key] = str(path)
    environment["QT_QPA_PLATFORM"] = "xcb"
    return environment


def read_peak_rss_kb(status: str) -> int:
    for line in status.splitlines():
        if line.startswith("VmHWM:"):
            return int(line.split()[1])
    raise ValueError("VmHWM is missing from /proc status")


def summarize_startup_trials(trials: list[dict[str, object]]) -> dict[str, object]:
    if not trials:
        raise ValueError("at least one startup trial is required")
    startup_times = [float(trial["window_visible_ms"]) for trial in trials]
    peak_rss = [int(trial["peak_rss_kb"]) for trial in trials]
    return {
        "trial_count": len(trials),
        "window_visible_median_ms": statistics.median(startup_times),
        "window_visible_min_ms": min(startup_times),
        "window_visible_max_ms": max(startup_times),
        "peak_rss_median_kb": statistics.median(peak_rss),
        "peak_rss_max_kb": max(peak_rss),
        "trials": trials,
    }


def _x11_windows() -> dict[str, str]:
    result = subprocess.run(
        ["xwininfo", "-root", "-tree"],
        check=True,
        capture_output=True,
        text=True,
    )
    windows = {}
    for line in result.stdout.splitlines():
        match = WINDOW_PATTERN.match(line)
        if match:
            windows[match.group(1)] = match.group(2)
    return windows


def _is_viewable(window_id: str) -> bool:
    result = subprocess.run(
        ["xwininfo", "-id", window_id],
        check=True,
        capture_output=True,
        text=True,
    )
    return "Map State: IsViewable" in result.stdout


def _wait_for_application_window(
    process: subprocess.Popen, before: set[str], timeout: float, started_at: float
) -> tuple[str, float]:
    deadline = started_at + timeout
    while time.perf_counter() < deadline:
        if process.poll() is not None:
            raise RuntimeError(f"Application exited before showing a window (exit {process.returncode}).")
        for window_id, title in _x11_windows().items():
            if window_id not in before and "LightMediaBrowser" in title and _is_viewable(window_id):
                return window_id, (time.perf_counter() - started_at) * 1000
        time.sleep(0.05)
    raise TimeoutError(f"LightMediaBrowser did not show a window within {timeout:g} seconds.")


def measure_application_startup(
    app_path: Path,
    environment: dict[str, str],
    timeout: float,
    settle_seconds: float,
    screenshot_path: Path | None = None,
) -> dict[str, object]:
    if screenshot_path is not None:
        screenshot_path = _reject_symlinked_path(screenshot_path, "screenshot")
    xwininfo = shutil.which("xwininfo")
    if not xwininfo:
        raise RuntimeError("xwininfo is required to measure window-visible startup on X11.")

    before = set(_x11_windows())
    log_path = Path(environment["XDG_DATA_HOME"]).parent / "application.log"
    with log_path.open("w", encoding="utf-8") as log_file:
        started_at = time.perf_counter()
        process = subprocess.Popen(
            [str(app_path)],
            cwd=ROOT,
            env=environment,
            stdout=log_file,
            stderr=subprocess.STDOUT,
        )
        try:
            window_id, startup_ms = _wait_for_application_window(process, before, timeout, started_at)
            time.sleep(settle_seconds)
            status_path = Path(f"/proc/{process.pid}/status")
            peak_rss_kb = read_peak_rss_kb(status_path.read_text(encoding="utf-8"))
            result: dict[str, object] = {
                "window_visible_ms": startup_ms,
                "peak_rss_kb": peak_rss_kb,
                "window_title": _x11_windows().get(window_id, ""),
            }
            if screenshot_path is not None:
                screenshot_path.parent.mkdir(parents=True, exist_ok=True)
                try:
                    subprocess.run(
                        ["import", "-window", window_id, str(screenshot_path)],
                        check=True,
                        capture_output=True,
                        text=True,
                        timeout=20,
                    )
                    result["screenshot"] = str(screenshot_path)
                except (FileNotFoundError, subprocess.CalledProcessError, subprocess.TimeoutExpired) as exception:
                    result["screenshot_error"] = str(exception)
            return result
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()


def _system_details() -> dict[str, object]:
    cpu_model = ""
    for line in Path("/proc/cpuinfo").read_text(encoding="utf-8").splitlines():
        if line.startswith("model name"):
            cpu_model = line.partition(":")[2].strip()
            break

    memory_kb = None
    for line in Path("/proc/meminfo").read_text(encoding="utf-8").splitlines():
        if line.startswith("MemTotal:"):
            memory_kb = int(line.split()[1])
            break

    return {
        "platform": platform.platform(),
        "machine": platform.machine(),
        "cpu_model": cpu_model or platform.processor(),
        "logical_cpu_count": os.cpu_count(),
        "memory_kb": memory_kb,
    }


def _parse_counts(value: str) -> list[int]:
    try:
        counts = [int(item) for item in value.split(",")]
    except ValueError as exception:
        raise argparse.ArgumentTypeError("counts must be comma-separated integers") from exception
    if not counts or counts[-1] > MAX_VIDEO_COUNT or any(count < 1 for count in counts):
        raise argparse.ArgumentTypeError(f"counts must be between 1 and {MAX_VIDEO_COUNT}")
    if counts != sorted(set(counts)):
        raise argparse.ArgumentTypeError("counts must be unique and in ascending order")
    return counts


def create_run_directory(dataset_directory: Path) -> Path:
    dataset_directory = dataset_directory.resolve()
    if not dataset_directory.is_dir():
        raise ValueError(f"dataset directory does not exist: {dataset_directory}")

    runs_directory = dataset_directory / "runs"
    if runs_directory.is_symlink():
        raise ValueError(f"refusing symlinked benchmark runs directory: {runs_directory}")
    runs_directory.mkdir(exist_ok=True)
    if runs_directory.resolve().parent != dataset_directory:
        raise ValueError(f"benchmark runs directory escapes the dataset directory: {runs_directory}")

    run_directory = runs_directory / (
        datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "-" + uuid.uuid4().hex[:8]
    )
    run_directory.mkdir()
    try:
        run_directory.resolve().relative_to(dataset_directory)
    except ValueError as exception:
        raise ValueError(f"benchmark run directory escapes the dataset directory: {run_directory}") from exception
    return run_directory


def run_benchmark(arguments: argparse.Namespace) -> Path:
    if arguments.startup_trials < 1:
        raise ValueError("startup trials must be at least one")
    if arguments.settle_seconds < 0:
        raise ValueError("settle seconds must not be negative")

    screenshot_path = (
        _reject_symlinked_path(arguments.screenshot_path, "screenshot")
        if arguments.screenshot_path
        else None
    )
    output_path = _reject_symlinked_path(arguments.output, "report") if arguments.output else None

    app_path = arguments.app.resolve()
    benchmark_path = arguments.database_benchmark.resolve()
    for executable in (app_path, benchmark_path):
        if not executable.is_file() or not os.access(executable, os.X_OK):
            raise FileNotFoundError(f"benchmark executable is missing or not executable: {executable}")

    dataset_directory = arguments.dataset_dir.resolve()
    if dataset_directory.exists():
        video_directory, sample_video = validate_dataset(dataset_directory, arguments.counts[-1])
    else:
        video_directory, sample_video = generate_dataset(dataset_directory, arguments.counts[-1])

    run_directory = create_run_directory(dataset_directory)

    database_environment = create_isolated_environment(run_directory / "database")
    database_process = subprocess.run(
        [
            str(benchmark_path),
            "--video-dir",
            str(video_directory),
            "--counts",
            ",".join(str(count) for count in arguments.counts),
        ],
        cwd=ROOT,
        env=database_environment,
        check=True,
        capture_output=True,
        text=True,
    )
    database_report = json.loads(database_process.stdout)

    empty_environment = create_isolated_environment(run_directory / "empty-startup")
    empty_startup = measure_application_startup(
        app_path, empty_environment, arguments.startup_timeout, arguments.settle_seconds
    )
    empty_relaunches = [
        measure_application_startup(
            app_path, empty_environment, arguments.startup_timeout, arguments.settle_seconds
        )
        for _ in range(arguments.startup_trials)
    ]

    populated_startups = []
    for trial in range(arguments.startup_trials):
        populated_startups.append(measure_application_startup(
            app_path,
            database_environment,
            arguments.startup_timeout,
            arguments.settle_seconds,
            screenshot_path if trial == 0 else None,
        ))

    result = {
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "system": _system_details(),
        "dataset": {
            "video_count": arguments.counts[-1],
            "sample_video": str(sample_video),
            "description": "One one-second MP4 hard-linked under unique file names.",
        },
        "database": database_report,
        "startup": {
            "new_empty_database_first_launch": empty_startup,
            "empty_database_relaunches": summarize_startup_trials(empty_relaunches),
            "populated_database_launches": summarize_startup_trials(populated_startups),
            "settle_seconds_before_peak_rss_read": arguments.settle_seconds,
        },
    }
    output_path = (
        _reject_symlinked_path(output_path, "report")
        if output_path
        else run_directory / "results.json"
    )
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(output_path)
    print(json.dumps(result, indent=2))
    return output_path


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate an isolated dataset and run LightMediaBrowser benchmarks.")
    parser.add_argument("--app", type=Path, required=True, help="Path to the built LightMediaBrowser app.")
    parser.add_argument(
        "--database-benchmark",
        type=Path,
        required=True,
        help="Path to lightmediabrowser_database_benchmark.",
    )
    parser.add_argument(
        "--dataset-dir",
        type=Path,
        default=Path(__file__).resolve().parent / "generated",
    )
    parser.add_argument("--counts", type=_parse_counts, default=_parse_counts("100,1000,10000"))
    parser.add_argument("--startup-timeout", type=float, default=30)
    parser.add_argument("--settle-seconds", type=float, default=2)
    parser.add_argument("--startup-trials", type=int, default=5)
    parser.add_argument("--screenshot-path", type=Path)
    parser.add_argument("--output", type=Path)
    arguments = parser.parse_args()

    try:
        run_benchmark(arguments)
    except (OSError, RuntimeError, TimeoutError, ValueError, subprocess.CalledProcessError) as exception:
        print(f"Benchmark failed: {exception}", file=sys.stderr)
        if isinstance(exception, subprocess.CalledProcessError) and exception.stderr:
            print(exception.stderr, file=sys.stderr)
        raise SystemExit(1) from exception


if __name__ == "__main__":
    main()
