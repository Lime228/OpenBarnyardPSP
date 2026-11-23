#!/usr/bin/env python3
"""Batch-convert installed Bink files to a bounded, sequential PSP movie cache."""

import argparse
import json
import struct
import subprocess
import tempfile
from contextlib import ExitStack
from fractions import Fraction
from pathlib import Path


MAGIC = b"PMVF"
VERSION = 2
WIDTH, HEIGHT = 480, 272
MAX_FPS = 30
SAMPLE_RATE = 44100
PCM_SAMPLE_BYTES = 4  # Stereo signed PCM16.
MAX_JPEG_BYTES = 64 * 1024
MAX_AUDIO_TRACKS = 4
HEADER_V1 = struct.Struct("<4s10I")
HEADER = struct.Struct("<4s11I")
PREFIX = struct.Struct("<4s2I")
FRAME_SIZE = struct.Struct("<I")


def command(executable: Path, *arguments: str, timeout: int = 300) -> bytes:
    process = subprocess.run(
        [str(executable), *arguments],
        capture_output=True,
        timeout=timeout,
        check=False,
        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
    )
    if process.returncode:
        tail = process.stderr[-1800:].decode(errors="replace").strip()
        raise ValueError(f"{executable.stem} exited with {process.returncode}: {tail}")
    return process.stdout


def validate_jpeg(data: bytes) -> None:
    if not (data.startswith(b"\xff\xd8") and data.endswith(b"\xff\xd9")):
        raise ValueError("invalid JPEG packet markers")
    offset = 2
    has_frame = False
    required_tables = {0xC4, 0xDB}  # Huffman and quantisation tables.
    while offset + 4 <= len(data):
        if data[offset] != 0xFF:
            raise ValueError("invalid JPEG marker")
        marker = data[offset + 1]
        size = int.from_bytes(data[offset + 2 : offset + 4], "big")
        if size < 2 or offset + 2 + size > len(data):
            raise ValueError("invalid JPEG section size")
        content = data[offset + 4 : offset + 2 + size]
        required_tables.discard(marker)
        if marker == 0xC0:
            # Baseline Y/Cb/Cr, with 2x2 luma and 1x1 chroma sampling.
            if len(content) != 15 or content[0] != 8 or content[5] != 3:
                raise ValueError("invalid baseline JPEG components")
            height, width = struct.unpack_from(">HH", content, 1)
            if (width, height) != (WIDTH, HEIGHT):
                raise ValueError("JPEG frame dimensions disagree with PMVF profile")
            if tuple(content[index] for index in (7, 10, 13)) != (0x22, 0x11, 0x11):
                raise ValueError("JPEG frame must use 4:2:0 sampling")
            has_frame = True
        elif marker == 0xDA:
            if not has_frame or required_tables:
                raise ValueError("JPEG packet must carry its own baseline frame and tables")
            return
        offset += 2 + size
    raise ValueError("missing JPEG scan")


def index_packets(document: dict, file_bytes: int) -> list[tuple[int, int]]:
    packets = []
    expected = 0
    for packet in document.get("packets", []):
        position, size = int(packet["pos"]), int(packet["size"])
        if position != expected or size <= 0 or position + size > file_bytes:
            raise ValueError("invalid or discontinuous MJPEG packet index")
        packets.append((position, size))
        expected += size
    if not packets or expected != file_bytes:
        raise ValueError("MJPEG packet index does not cover the complete video")
    return packets


def psp_frame_rate(value: str, max_fps: int) -> Fraction:
    try:
        rate = min(Fraction(value), max_fps)
    except (TypeError, ValueError, ZeroDivisionError) as error:
        raise ValueError(f"invalid source video rate: {value!r}") from error
    rate = Fraction(rate)
    if not 1 <= rate <= MAX_FPS or max(rate.numerator, rate.denominator) >= 2**32:
        raise ValueError(f"unsupported PMVF video rate: {rate}")
    return rate


def audio_bytes_at(frame: int, fps: Fraction) -> int:
    return frame * SAMPLE_RATE * fps.denominator // fps.numerator * PCM_SAMPLE_BYTES


def max_pcm_bytes(fps: Fraction) -> int:
    return ((SAMPLE_RATE * fps.denominator + fps.numerator - 1) // fps.numerator) * PCM_SAMPLE_BYTES


def verify_movie(path: Path) -> None:
    with path.open("rb") as movie:
        try:
            magic, version, header_bytes = PREFIX.unpack(movie.read(PREFIX.size))
            layout = {1: HEADER_V1, VERSION: HEADER}.get(version)
            if magic != MAGIC or layout is None or header_bytes != layout.size:
                raise ValueError("unsupported PMVF header")
            movie.seek(0)
            _, _, _, width, height, fps_num, *fields = layout.unpack(movie.read(header_bytes))
            fps_den = fields.pop(0) if version == VERSION else 1
            frames, max_jpeg, tracks, rate, pcm = fields
            fps = Fraction(fps_num, fps_den)
        except (struct.error, ZeroDivisionError) as error:
            raise ValueError("truncated PMVF header") from error
        if (width, height) != (WIDTH, HEIGHT) or not 1 <= fps <= MAX_FPS or (version == 1 and fps != 15):
            raise ValueError("unsupported PMVF header")
        if not (tracks <= MAX_AUDIO_TRACKS and 0 < max_jpeg <= MAX_JPEG_BYTES):
            raise ValueError("invalid PMVF frame or track bounds")
        if (rate, pcm) != ((SAMPLE_RATE, max_pcm_bytes(fps)) if tracks else (0, 0)):
            raise ValueError("invalid PMVF PCM layout")
        file_bytes = path.stat().st_size
        minimum_bytes = header_bytes + frames * (FRAME_SIZE.size + 1) + tracks * audio_bytes_at(frames, fps)
        if not frames or minimum_bytes > file_bytes:
            raise ValueError("invalid PMVF frame count")
        observed_max = 0
        audio_offset = 0
        for frame in range(1, frames + 1):
            next_audio_offset = audio_bytes_at(frame, fps)
            audio_bytes = tracks * (next_audio_offset - audio_offset)
            audio_offset = next_audio_offset
            size_data = movie.read(FRAME_SIZE.size)
            if len(size_data) != FRAME_SIZE.size:
                raise ValueError("truncated PMVF frame length")
            size = FRAME_SIZE.unpack(size_data)[0]
            if not 0 < size <= max_jpeg or movie.tell() + size + audio_bytes > file_bytes:
                raise ValueError("invalid or truncated PMVF frame")
            data = movie.read(size)
            validate_jpeg(data)
            movie.seek(audio_bytes, 1)
            observed_max = max(size, observed_max)
        if movie.tell() != file_bytes or observed_max != max_jpeg:
            raise ValueError("PMVF length or declared maximum frame size differs from its records")


def pack_movie(jpeg: Path, packets: list[tuple[int, int]], audio: list[Path], output: Path, fps: Fraction) -> dict:
    frame_count = len(packets)
    max_frame = max(size for _, size in packets)
    if max_frame > MAX_JPEG_BYTES or len(audio) > MAX_AUDIO_TRACKS:
        raise ValueError("movie exceeds the PMVF frame or track budget")
    pcm_sizes = [path.stat().st_size for path in audio]
    if any(size % PCM_SAMPLE_BYTES for size in pcm_sizes):
        raise ValueError("audio contains an incomplete PCM sample frame")
    header = HEADER.pack(
        MAGIC, VERSION, HEADER.size, WIDTH, HEIGHT, fps.numerator, fps.denominator, frame_count, max_frame,
        len(audio), SAMPLE_RATE if audio else 0, max_pcm_bytes(fps) if audio else 0,
    )
    target_audio_bytes = audio_bytes_at(frame_count, fps)
    expected_size = HEADER.size + jpeg.stat().st_size + frame_count * FRAME_SIZE.size + len(audio) * target_audio_bytes
    peak_window = 0
    window = 0
    frames_per_window = (fps.numerator + fps.denominator - 1) // fps.denominator
    with ExitStack() as stack:
        video = stack.enter_context(jpeg.open("rb"))
        tracks = [stack.enter_context(track.open("rb")) for track in audio]
        movie = stack.enter_context(output.open("wb"))
        movie.write(header)
        for frame, (position, size) in enumerate(packets):
            if video.tell() != position:
                raise ValueError("MJPEG input disagrees with the packet index")
            data = video.read(size)
            if len(data) != size:
                raise ValueError("truncated MJPEG packet")
            validate_jpeg(data)
            movie.write(FRAME_SIZE.pack(size))
            movie.write(data)
            pcm_bytes = audio_bytes_at(frame + 1, fps) - audio_bytes_at(frame, fps)
            for track in tracks:
                pcm = track.read(pcm_bytes)
                movie.write(pcm)
                movie.write(bytes(pcm_bytes - len(pcm)))
            window += size + FRAME_SIZE.size + len(audio) * pcm_bytes
            if frame >= frames_per_window:
                first = frame - frames_per_window
                discarded_pcm = audio_bytes_at(first + 1, fps) - audio_bytes_at(first, fps)
                window -= packets[first][1] + FRAME_SIZE.size + len(audio) * discarded_pcm
            peak_window = max(peak_window, window)
        if movie.tell() != expected_size:
            raise ValueError("PMVF length disagrees with its frame index")

    return {
        "width": WIDTH, "height": HEIGHT, "fps": float(fps), "frame_rate": str(fps), "frames": frame_count,
        "max_jpeg_bytes": max_frame, "peak_window_bytes": peak_window, "bytes": expected_size,
        "audio": [
            {
                "decoded_frames": size // 4,
                "padded_frames": max(0, target_audio_bytes - size) // 4,
                "trimmed_frames": max(0, size - target_audio_bytes) // 4,
            }
            for size in pcm_sizes
        ],
    }


def convert_movie(ffmpeg: Path, ffprobe: Path, source: Path, output: Path, max_fps: int) -> dict:
    source_info = json.loads(command(
        ffprobe, "-v", "error", "-count_packets", "-show_streams", "-of", "json", str(source),
    ))
    video = [stream for stream in source_info["streams"] if stream["codec_type"] == "video"]
    audio = [stream for stream in source_info["streams"] if stream["codec_type"] == "audio"]
    if len(video) != 1 or len(audio) > MAX_AUDIO_TRACKS:
        raise ValueError("expected one video stream and at most four audio streams")
    fps = psp_frame_rate(video[0]["avg_frame_rate"], max_fps)
    source_frames = int(video[0]["nb_read_packets"])
    preserve_frames = fps == Fraction(video[0]["avg_frame_rate"])
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="movie-", dir=output.parent) as directory:
        scratch = Path(directory)
        images = scratch / "video.mjpeg"
        filters = (
            f"scale={WIDTH}:{HEIGHT}:force_original_aspect_ratio=decrease:"
            f"force_divisible_by=2,pad={WIDTH}:{HEIGHT}:(ow-iw)/2:(oh-ih)/2,setsar=1"
        )
        if not preserve_frames:
            filters = f"fps={fps},{filters}"
        for quality in (5, 7, 9):
            command(
                ffmpeg, "-v", "error", "-xerror", "-nostdin", "-y", "-threads", "2", "-i", str(source),
                "-map", "0:v:0", "-an", "-vf", filters, "-c:v", "mjpeg", "-pix_fmt", "yuvj420p",
                "-fps_mode", "passthrough", "-q:v", str(quality), "-threads", "2", "-f", "mjpeg", str(images),
            )
            packet_info = json.loads(command(
                ffprobe, "-v", "error", "-f", "mjpeg", "-show_packets",
                "-show_entries", "packet=pos,size", "-of", "json", str(images),
            ))
            packets = index_packets(packet_info, images.stat().st_size)
            if max(size for _, size in packets) <= MAX_JPEG_BYTES:
                break
        else:
            raise ValueError("JPEG frames exceed the 64 KiB PSP cache limit")
        if preserve_frames and len(packets) != source_frames:
            raise ValueError(f"encoder changed the number of original frames: {source_frames} to {len(packets)}")

        # Verify the complete encoded picture stream with an independent decoder.
        command(ffmpeg, "-v", "error", "-xerror", "-nostdin", "-f", "mjpeg", "-i", str(images),
                "-f", "null", "-")
        tracks = []
        for stream in audio:
            path = scratch / f"audio-{stream['index']}.pcm"
            command(
                ffmpeg, "-v", "error", "-xerror", "-nostdin", "-i", str(source),
                "-map", f"0:{stream['index']}", "-vn", "-ac", "2", "-ar", str(SAMPLE_RATE),
                "-c:a", "pcm_s16le", "-f", "s16le", str(path),
            )
            tracks.append(path)
        pending = scratch / "packed.pmvf"
        details = pack_movie(images, packets, tracks, pending, fps)
        verify_movie(pending)
        pending.replace(output)
    details["jpeg_quality"] = quality
    details["source_video"] = {
        key: video[0].get(key) for key in ("codec_name", "width", "height", "avg_frame_rate", "duration")
    }
    details["source_video"]["frames"] = source_frames
    for packed, original in zip(details["audio"], audio, strict=True):
        packed.update(source_stream=original["index"], source_channels=original.get("channels"),
                      source_sample_rate=original.get("sample_rate"))
    return details


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ffmpeg", default="ffmpeg")
    parser.add_argument("--ffprobe", default="ffprobe")
    parser.add_argument("--max-fps", type=int, default=MAX_FPS,
                        help=f"keep the source rate up to this ceiling (default: {MAX_FPS})")
    parser.add_argument("install", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    if not 1 <= args.max_fps <= MAX_FPS:
        parser.error(f"--max-fps must be between 1 and {MAX_FPS}")
    install = args.install.resolve(strict=True)
    output = args.output.resolve()
    if output.is_relative_to(install):
        parser.error("output must be outside the original installation")
    files = sorted(
        (path for path in install.rglob("*") if path.is_file() and path.suffix.casefold() == ".bik"),
        key=lambda path: path.relative_to(install).as_posix().casefold(),
    )
    if not files:
        parser.error("no installed Bink movies found")
    ffmpeg, ffprobe = Path(args.ffmpeg), Path(args.ffprobe)
    version = command(ffmpeg, "-version").decode(errors="replace").splitlines()[0]
    movies = []
    for source in files:
        relative = source.relative_to(install)
        destination = relative.with_suffix(".pmvf")
        result = {"source": relative.as_posix(), "status": "failed"}
        try:
            result.update(convert_movie(ffmpeg, ffprobe, source, output / destination, args.max_fps))
            result.update(status="converted", path=destination.as_posix())
        except (KeyError, OSError, ValueError, subprocess.TimeoutExpired) as error:
            result["error"] = str(error)
        movies.append(result)
    converted = [movie for movie in movies if movie["status"] == "converted"]
    report = {
        "version": VERSION, "ffmpeg": version, "max_fps": args.max_fps, "movie_count": len(files),
        "converted_count": len(converted), "bytes": sum(movie["bytes"] for movie in converted),
        "movies": movies,
    }
    output.mkdir(parents=True, exist_ok=True)
    pending = output / "movies.batch.json.tmp"
    pending.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    pending.replace(output / "movies.batch.json")
    print(f"{len(converted)}/{len(files)} movies converted")
    return int(len(converted) != len(files))


if __name__ == "__main__":
    raise SystemExit(main())
