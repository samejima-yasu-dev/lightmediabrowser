import json
import os
import sys
import subprocess
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from benchmarks.generate_dataset import MAX_VIDEO_COUNT, generate_dataset, validate_dataset


class BenchmarkDatasetTest(unittest.TestCase):
    def test_generates_requested_count_as_hard_links_to_a_short_mp4(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            output_directory = Path(temporary_directory) / "dataset"

            video_directory, sample_video = generate_dataset(output_directory, 3)

            videos = sorted(video_directory.rglob("*.mp4"))
            self.assertEqual(len(videos), 3)
            self.assertTrue(sample_video.is_file())
            self.assertGreater(sample_video.stat().st_size, 0)
            self.assertEqual(
                {os.stat(video).st_ino for video in videos},
                {sample_video.stat().st_ino},
            )
            probe = subprocess.run(
                [
                    "ffprobe",
                    "-v",
                    "error",
                    "-show_entries",
                    "stream=width,height:format=duration",
                    "-of",
                    "json",
                    str(sample_video),
                ],
                check=True,
                capture_output=True,
                text=True,
            )
            metadata = json.loads(probe.stdout)
            self.assertEqual(metadata["streams"][0]["width"], 640)
            self.assertEqual(metadata["streams"][0]["height"], 360)
            self.assertLessEqual(float(metadata["format"]["duration"]), 1.1)

    def test_rejects_counts_above_the_supported_maximum(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            with self.assertRaises(ValueError):
                generate_dataset(Path(temporary_directory) / "dataset", MAX_VIDEO_COUNT + 1)

    def test_does_not_overwrite_an_existing_output_directory(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            output_directory = Path(temporary_directory) / "dataset"
            output_directory.mkdir()
            sentinel = output_directory / "keep.txt"
            sentinel.write_text("keep")

            with self.assertRaises(FileExistsError):
                generate_dataset(output_directory, 1)

            self.assertEqual(sentinel.read_text(), "keep")

    def test_validates_dataset_count_and_hard_link_layout(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            output_directory = Path(temporary_directory) / "dataset"
            video_directory, sample_video = generate_dataset(output_directory, 2)

            self.assertEqual(validate_dataset(output_directory, 2), (video_directory, sample_video))

            video = next(video_directory.rglob("*.mp4"))
            video.unlink()
            video.write_bytes(sample_video.read_bytes())
            with self.assertRaises(ValueError):
                validate_dataset(output_directory, 2)

    def test_rejects_symbolic_links_instead_of_hard_links(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            output_directory = Path(temporary_directory) / "dataset"
            video_directory, sample_video = generate_dataset(output_directory, 2)
            video = next(video_directory.rglob("*.mp4"))
            video.unlink()
            video.symlink_to(sample_video)

            with self.assertRaises(ValueError):
                validate_dataset(output_directory, 2)


if __name__ == "__main__":
    unittest.main()
