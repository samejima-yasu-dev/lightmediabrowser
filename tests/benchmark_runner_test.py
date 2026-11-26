import os
import sys
import tempfile
import time
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from benchmarks.run_benchmark import (
    create_isolated_environment,
    create_run_directory,
    measure_application_startup,
    read_peak_rss_kb,
    summarize_startup_trials,
)


class BenchmarkRunnerTest(unittest.TestCase):
    def test_creates_private_xdg_directories_without_changing_parent_environment(self):
        original_data_home = os.environ.get("XDG_DATA_HOME")

        with tempfile.TemporaryDirectory() as temporary_directory:
            runtime_directory = Path(temporary_directory) / "runtime"
            environment = create_isolated_environment(runtime_directory)

            self.assertEqual(environment["XDG_DATA_HOME"], str(runtime_directory / "data"))
            self.assertEqual(environment["XDG_CONFIG_HOME"], str(runtime_directory / "config"))
            self.assertEqual(environment["XDG_CACHE_HOME"], str(runtime_directory / "cache"))
            self.assertEqual(environment["QT_QPA_PLATFORM"], "xcb")
            self.assertTrue(all(Path(environment[key]).is_dir() for key in (
                "XDG_DATA_HOME", "XDG_CONFIG_HOME", "XDG_CACHE_HOME"
            )))

        self.assertEqual(os.environ.get("XDG_DATA_HOME"), original_data_home)

    def test_reads_linux_peak_rss_from_proc_status(self):
        status = "Name:\tapp\nVmRSS:\t12000 kB\nVmHWM:\t18432 kB\n"

        self.assertEqual(read_peak_rss_kb(status), 18432)

    def test_fails_when_peak_rss_is_unavailable(self):
        with self.assertRaises(ValueError):
            read_peak_rss_kb("Name:\tapp\nVmRSS:\t12000 kB\n")

    def test_summarizes_repeated_startup_and_memory_measurements(self):
        trials = [
            {"window_visible_ms": 100.0, "peak_rss_kb": 500},
            {"window_visible_ms": 300.0, "peak_rss_kb": 700},
            {"window_visible_ms": 200.0, "peak_rss_kb": 600},
        ]

        summary = summarize_startup_trials(trials)

        self.assertEqual(summary["trial_count"], 3)
        self.assertEqual(summary["window_visible_median_ms"], 200.0)
        self.assertEqual(summary["peak_rss_median_kb"], 600)
        self.assertEqual(summary["peak_rss_max_kb"], 700)
        self.assertEqual(summary["trials"], trials)

    def test_startup_time_includes_process_creation(self):
        class FakeProcess:
            pid = os.getpid()

            def __init__(self):
                self.running = True

            def poll(self):
                return None if self.running else 0

            def terminate(self):
                self.running = False

            def wait(self, timeout=None):
                return 0

        with tempfile.TemporaryDirectory() as temporary_directory:
            environment = create_isolated_environment(Path(temporary_directory) / "runtime")
            process = FakeProcess()

            def delayed_process_creation(*args, **kwargs):
                time.sleep(0.06)
                return process

            with (
                patch("benchmarks.run_benchmark.shutil.which", return_value="/usr/bin/xwininfo"),
                patch("benchmarks.run_benchmark._x11_windows",
                      side_effect=[{}, {"0x1": "LightMediaBrowser"},
                                   {"0x1": "LightMediaBrowser"}]),
                patch("benchmarks.run_benchmark._is_viewable", return_value=True),
                patch("benchmarks.run_benchmark.subprocess.Popen", side_effect=delayed_process_creation),
                patch("benchmarks.run_benchmark.read_peak_rss_kb", return_value=123),
            ):
                result = measure_application_startup(Path("/fake/app"), environment, 1, 0)

            self.assertGreaterEqual(result["window_visible_ms"], 50)

    def test_refuses_a_symlinked_run_directory(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            dataset = Path(temporary_directory) / "dataset"
            outside = Path(temporary_directory) / "outside"
            dataset.mkdir()
            outside.mkdir()
            (dataset / "runs").symlink_to(outside, target_is_directory=True)

            with self.assertRaises(ValueError):
                create_run_directory(dataset)

            self.assertEqual(list(outside.iterdir()), [])

    def test_refuses_screenshot_paths_with_symlinked_parent_directories(self):
        class FakeProcess:
            pid = os.getpid()

            def poll(self):
                return None

            def terminate(self):
                pass

            def wait(self, timeout=None):
                return 0

        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            outside = root / "outside"
            outside.mkdir()
            alias = root / "alias"
            alias.symlink_to(outside, target_is_directory=True)
            environment = create_isolated_environment(root / "runtime")

            with (
                patch("benchmarks.run_benchmark.shutil.which", return_value="/usr/bin/xwininfo"),
                patch("benchmarks.run_benchmark._x11_windows",
                      side_effect=[{}, {"0x1": "LightMediaBrowser"},
                                   {"0x1": "LightMediaBrowser"}]),
                patch("benchmarks.run_benchmark._is_viewable", return_value=True),
                patch("benchmarks.run_benchmark.subprocess.Popen", return_value=FakeProcess()),
                patch("benchmarks.run_benchmark.subprocess.run", return_value=None),
                patch("benchmarks.run_benchmark.read_peak_rss_kb", return_value=123),
            ):
                with self.assertRaises(ValueError):
                    measure_application_startup(
                        Path("/fake/app"),
                        environment,
                        1,
                        0,
                        alias / "screenshot.png",
                    )

            self.assertEqual(list(outside.iterdir()), [])


if __name__ == "__main__":
    unittest.main()
