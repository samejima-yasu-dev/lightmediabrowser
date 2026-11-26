import argparse
import stat
import subprocess
from pathlib import Path


MAX_VIDEO_COUNT = 10_000
VIDEOS_PER_DIRECTORY = 100
PROJECT_ROOT = Path(__file__).resolve().parents[1]


def generate_dataset(output_directory: Path, count: int, ffmpeg: str = "ffmpeg") -> tuple[Path, Path]:
    if count < 1 or count > MAX_VIDEO_COUNT:
        raise ValueError(f"count must be between 1 and {MAX_VIDEO_COUNT}")
    if output_directory.exists():
        raise FileExistsError(f"output directory already exists: {output_directory}")

    output_directory.mkdir(parents=True)
    sample_video = output_directory / "sample.mp4"
    video_directory = output_directory / "videos"
    video_directory.mkdir()

    subprocess.run(
        [
            ffmpeg,
            "-hide_banner",
            "-loglevel",
            "error",
            "-loop",
            "1",
            "-framerate",
            "10",
            "-i",
            str(PROJECT_ROOT / "resources" / "lightmediabrowser.png"),
            "-t",
            "1",
            "-vf",
            "scale=640:360:force_original_aspect_ratio=decrease,"
            "pad=640:360:(ow-iw)/2:(oh-ih)/2:color=0x162033,format=yuv420p",
            "-an",
            "-c:v",
            "mpeg4",
            "-q:v",
            "5",
            "-movflags",
            "+faststart",
            str(sample_video),
        ],
        check=True,
    )

    for index in range(1, count + 1):
        shard = video_directory / f"{(index - 1) // VIDEOS_PER_DIRECTORY:04d}"
        shard.mkdir(exist_ok=True)
        video_path = shard / f"sample-{index:05d}.mp4"
        video_path.hardlink_to(sample_video)

    return video_directory, sample_video


def validate_dataset(output_directory: Path, count: int) -> tuple[Path, Path]:
    if count < 1 or count > MAX_VIDEO_COUNT:
        raise ValueError(f"count must be between 1 and {MAX_VIDEO_COUNT}")

    sample_video = output_directory / "sample.mp4"
    video_directory = output_directory / "videos"
    if not sample_video.is_file() or not video_directory.is_dir():
        raise ValueError(f"dataset is missing sample.mp4 or videos/: {output_directory}")

    videos = list(video_directory.rglob("*.mp4"))
    if len(videos) != count:
        raise ValueError(f"expected {count} video files, found {len(videos)}")

    sample_stat = sample_video.lstat()
    if not stat.S_ISREG(sample_stat.st_mode):
        raise ValueError(f"sample.mp4 is not a regular file: {sample_video}")
    for video in videos:
        video_stat = video.lstat()
        if (
            not stat.S_ISREG(video_stat.st_mode)
            or video_stat.st_dev != sample_stat.st_dev
            or video_stat.st_ino != sample_stat.st_ino
        ):
            raise ValueError(f"video is not a hard link to sample.mp4: {video}")

    return video_directory, sample_video


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate a synthetic video library for benchmarks.")
    parser.add_argument("--count", type=int, default=MAX_VIDEO_COUNT)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path(__file__).resolve().parent / "generated",
    )
    arguments = parser.parse_args()

    videos, sample = generate_dataset(arguments.output_dir, arguments.count)
    print(f"Generated {arguments.count} hard-linked videos in {videos}")
    print(f"Short MP4 sample: {sample}")


if __name__ == "__main__":
    main()
