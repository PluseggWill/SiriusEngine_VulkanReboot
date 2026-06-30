#!/usr/bin/env python3
"""Bake default IBL assets from a Poly Haven HDRI (CC0).

Coordinate contract:
- Engine world: Z-up (+Z sky, +Y forward), matching Vk_Camera / Sponza.
- Poly Haven HDRIs: Blender Y-up (-Z forward).
- Cubemap faces are authored in engine space; equirect sampling converts via engine_dir_to_hdri().
"""

from __future__ import annotations

import argparse
import json
import math
import struct
import urllib.request
import zlib
from pathlib import Path

import numpy as np

FACE_ORDER = ("posx", "negx", "posy", "negy", "posz", "negz")
POLYHAVEN_USER_AGENT = "SiriusEngine-VulkanReboot/1.0 (offline IBL bake)"
DEFAULT_HDRI_SLUG = "kloppenheim_02"


def normalize(v: tuple[float, float, float]) -> tuple[float, float, float]:
    x, y, z = v
    length = math.sqrt(x * x + y * y + z * z)
    if length < 1e-8:
        return (0.0, 0.0, 1.0)
    return (x / length, y / length, z / length)


def cubemap_direction(face: str, u: float, v: float) -> tuple[float, float, float]:
    """Vulkan cubemap face directions in engine space (Z-up world)."""
    uc = u * 2.0 - 1.0
    vc = v * 2.0 - 1.0
    if face == "posx":
        d = (1.0, -vc, -uc)
    elif face == "negx":
        d = (-1.0, -vc, uc)
    elif face == "posy":
        d = (uc, 1.0, vc)
    elif face == "negy":
        d = (uc, -1.0, -vc)
    elif face == "posz":
        d = (uc, -vc, 1.0)
    else:
        d = (-uc, -vc, -1.0)
    return normalize(d)


def engine_dir_to_hdri(direction: tuple[float, float, float]) -> tuple[float, float, float]:
    """Engine world is Z-up (+Z sky); Poly Haven HDRIs are Blender Y-up (-Z forward)."""
    x, y, z = normalize(direction)
    return normalize((x, z, -y))


def direction_to_equirect_uv(direction: tuple[float, float, float]) -> tuple[float, float]:
    x, y, z = normalize(direction)
    u = math.atan2(z, x) / (2.0 * math.pi) + 0.5
    # Radiance/Poly Haven store rows with -Y; flip so +Y samples the zenith row.
    v = 0.5 - math.asin(max(-1.0, min(1.0, y))) / math.pi
    return u, v


def sample_equirect_bilinear(hdr: np.ndarray, engine_direction: tuple[float, float, float]) -> tuple[float, float, float]:
    hdri_direction = engine_dir_to_hdri(engine_direction)
    u, v = direction_to_equirect_uv(hdri_direction)
    height, width = hdr.shape[:2]
    fx = u * width - 0.5
    fy = v * height - 0.5
    x0 = int(math.floor(fx)) % width
    y0 = max(0, min(height - 1, int(math.floor(fy))))
    x1 = (x0 + 1) % width
    y1 = max(0, min(height - 1, y0 + 1))
    tx = fx - math.floor(fx)
    ty = fy - math.floor(fy)

    c00 = hdr[y0, x0]
    c10 = hdr[y0, x1]
    c01 = hdr[y1, x0]
    c11 = hdr[y1, x1]
    c0 = c00 * (1.0 - tx) + c10 * tx
    c1 = c01 * (1.0 - tx) + c11 * tx
    color = c0 * (1.0 - ty) + c1 * ty
    return float(color[0]), float(color[1]), float(color[2])


def sample_equirect_scaled(hdr: np.ndarray, direction: tuple[float, float, float], scale: float) -> tuple[float, float, float]:
    r, g, b = sample_equirect_bilinear(hdr, direction)
    return r * scale, g * scale, b * scale


def linear_to_srgb(c: float) -> float:
    c = max(0.0, min(1.0, c))
    if c <= 0.0031308:
        return 12.92 * c
    return 1.055 * (c ** (1.0 / 2.4)) - 0.055


def linear_to_byte(c: float) -> int:
    return int(max(0, min(255, round(c * 255.0))))


def compute_exposure_scale(hdr: np.ndarray, percentile: float = 99.0, target_peak: float = 0.85) -> float:
    luminance = 0.2126 * hdr[..., 0] + 0.7152 * hdr[..., 1] + 0.0722 * hdr[..., 2]
    peak = float(np.percentile(luminance, percentile))
    return target_peak / max(peak, 1e-4)


def decode_rgbe_scanline(data: memoryview, offset: int, width: int) -> tuple[np.ndarray, int]:
    rgbe = np.zeros((width, 4), dtype=np.uint8)
    channel = 0
    pos = 0
    while channel < 4:
        if offset >= len(data):
            raise RuntimeError("Unexpected end of HDR scanline data")
        marker = data[offset]
        offset += 1
        if marker > 128:
            count = marker - 128
            if offset >= len(data):
                raise RuntimeError("Unexpected end of HDR RLE run")
            value = data[offset]
            offset += 1
            rgbe[pos : pos + count, channel] = value
            pos += count
        else:
            count = marker
            end = offset + count
            if end > len(data):
                raise RuntimeError("Unexpected end of HDR RLE literal run")
            rgbe[pos : pos + count, channel] = np.frombuffer(data, dtype=np.uint8, count=count, offset=offset)
            offset = end
            pos += count
        if pos >= width:
            pos = 0
            channel += 1
    return rgbe, offset


def load_radiance_hdr(path: Path) -> np.ndarray:
    data = path.read_bytes()
    header_end = data.find(b"\n\n")
    if header_end < 0:
        raise RuntimeError(f"Invalid Radiance HDR header: {path}")

    resolution_pos = data.find(b"\n-Y ", header_end)
    if resolution_pos < 0:
        raise RuntimeError(f"Missing HDR resolution line: {path}")
    resolution_line_end = data.find(b"\n", resolution_pos + 1)
    if resolution_line_end < 0:
        raise RuntimeError(f"Truncated HDR resolution line: {path}")

    resolution_line = data[resolution_pos + 1 : resolution_line_end].decode("ascii", errors="ignore")
    parts = resolution_line.split()
    if len(parts) != 4 or parts[0] != "-Y" or parts[2] != "+X":
        raise RuntimeError(f"Unexpected HDR resolution line: {resolution_line}")
    height = int(parts[1])
    width = int(parts[3])

    binary = memoryview(data)[resolution_line_end + 1 :]
    offset = 0
    if offset + 4 > len(binary):
        raise RuntimeError(f"Truncated HDR payload: {path}")
    if binary[offset] != 0x02 or binary[offset + 1] != 0x02:
        raise RuntimeError("Only RGBE RLE scanline HDR files are supported")
    scanline_width = int.from_bytes(binary[offset + 2 : offset + 4], "big")
    if scanline_width != width:
        raise RuntimeError(f"HDR width mismatch: header={width} scanline={scanline_width}")
    offset = 4

    pixels = np.zeros((height, width, 3), dtype=np.float32)
    for y in range(height):
        if y > 0:
            if binary[offset] != 0x02 or binary[offset + 1] != 0x02:
                raise RuntimeError("Invalid RGBE scanline marker")
            scanline_width = int.from_bytes(binary[offset + 2 : offset + 4], "big")
            if scanline_width != width:
                raise RuntimeError("Inconsistent HDR scanline width")
            offset += 4
        rgbe, offset = decode_rgbe_scanline(binary, offset, width)
        exp = np.where(rgbe[:, 3] > 0, np.ldexp(1.0, rgbe[:, 3].astype(np.int32) - 128), 0.0)
        pixels[y, :, 0] = (rgbe[:, 0] / 255.0) * exp
        pixels[y, :, 1] = (rgbe[:, 1] / 255.0) * exp
        pixels[y, :, 2] = (rgbe[:, 2] / 255.0) * exp

    return pixels


def download_hdri(slug: str, resolution: str, cache_path: Path) -> Path:
    cache_path.parent.mkdir(parents=True, exist_ok=True)
    if cache_path.is_file():
        print(f"Using cached HDRI: {cache_path}")
        return cache_path

    url = f"https://dl.polyhaven.org/file/ph-assets/HDRIs/hdr/{resolution}/{slug}_{resolution}.hdr"
    print(f"Downloading HDRI: {url}")
    request = urllib.request.Request(url, headers={"User-Agent": POLYHAVEN_USER_AGENT})
    with urllib.request.urlopen(request, timeout=120) as response:
        cache_path.write_bytes(response.read())
    print(f"Saved HDRI to {cache_path}")
    return cache_path


def write_png_rgba8(path: Path, width: int, height: int, pixels: list[tuple[int, int, int, int]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        for x in range(width):
            r, g, b, a = pixels[y * width + x]
            raw.extend((r, g, b, a))

    def chunk(tag: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b"")
    path.write_bytes(png)


def hammersley(i: int, n: int) -> tuple[float, float]:
    bits = i
    bits = (bits << 16) | bits
    bits = ((bits & 0x55555555) << 1) | ((bits & 0xAAAAAAAA) >> 1)
    bits = ((bits & 0x33333333) << 2) | ((bits & 0xCCCCCCCC) >> 2)
    bits = ((bits & 0x0F0F0F0F) << 4) | ((bits & 0xF0F0F0F0) >> 4)
    bits = ((bits & 0x00FF00FF) << 8) | ((bits & 0xFF00FF00) >> 8)
    radical = float(bits) * 2.3283064365386963e-10
    return float(i) / float(n), radical


def build_tangent_frame(n: tuple[float, float, float]) -> tuple[tuple[float, float, float], tuple[float, float, float]]:
    up = (0.0, 1.0, 0.0) if abs(n[1]) < 0.999 else (1.0, 0.0, 0.0)
    tangent = normalize((up[1] * n[2] - up[2] * n[1], up[2] * n[0] - up[0] * n[2], up[0] * n[1] - up[1] * n[0]))
    bitangent = (n[1] * tangent[2] - n[2] * tangent[1], n[2] * tangent[0] - n[0] * tangent[2], n[0] * tangent[1] - n[1] * tangent[0])
    return tangent, bitangent


def hemisphere_sample(xi: tuple[float, float]) -> tuple[float, float, float]:
    phi = 2.0 * math.pi * xi[0]
    cos_theta = math.sqrt(1.0 - xi[1])
    sin_theta = math.sqrt(max(0.0, 1.0 - cos_theta * cos_theta))
    return math.cos(phi) * sin_theta, math.sin(phi) * sin_theta, cos_theta


def transform_tangent_vector(
    tangent: tuple[float, float, float], bitangent: tuple[float, float, float], n: tuple[float, float, float], local: tuple[float, float, float]
) -> tuple[float, float, float]:
    return normalize(
        (
            tangent[0] * local[0] + bitangent[0] * local[1] + n[0] * local[2],
            tangent[1] * local[0] + bitangent[1] * local[1] + n[1] * local[2],
            tangent[2] * local[0] + bitangent[2] * local[1] + n[2] * local[2],
        )
    )


def importance_sample_ggx(xi: tuple[float, float], roughness: float, n: tuple[float, float, float]) -> tuple[float, float, float]:
    a = roughness * roughness
    phi = 2.0 * math.pi * xi[0]
    cos_theta = math.sqrt((1.0 - xi[1]) / (1.0 + (a * a - 1.0) * xi[1]))
    sin_theta = math.sqrt(max(0.0, 1.0 - cos_theta * cos_theta))
    h_tangent = (math.cos(phi) * sin_theta, math.sin(phi) * sin_theta, cos_theta)
    tangent, bitangent = build_tangent_frame(n)
    return transform_tangent_vector(tangent, bitangent, n, h_tangent)


def reflect_vector(v: tuple[float, float, float], h: tuple[float, float, float]) -> tuple[float, float, float]:
    vdoth = v[0] * h[0] + v[1] * h[1] + v[2] * h[2]
    return normalize((2.0 * vdoth * h[0] - v[0], 2.0 * vdoth * h[1] - v[1], 2.0 * vdoth * h[2] - v[2]))


def integrate_brdf(ndotv: float, roughness: float, sample_count: int = 128) -> tuple[float, float]:
    v = (math.sqrt(max(0.0, 1.0 - ndotv * ndotv)), 0.0, ndotv)
    a = 0.0
    b = 0.0
    for i in range(sample_count):
        xi = hammersley(i, sample_count)
        h = importance_sample_ggx(xi, roughness, (0.0, 0.0, 1.0))
        l = reflect_vector(v, h)
        ndotl = max(0.0, l[2])
        ndoth = max(0.0, h[2])
        vdoth = max(0.0, v[0] * h[0] + v[1] * h[1] + v[2] * h[2])
        if ndotl <= 0.0:
            continue
        k = (roughness + 1.0) ** 2 / 8.0
        g_v = ndotv / max(ndotv * (1.0 - k) + k, 1e-4)
        g_l = ndotl / max(ndotl * (1.0 - k) + k, 1e-4)
        g_vis = (g_v * g_l * vdoth) / max(ndoth * ndotv, 1e-4)
        fc = (1.0 - vdoth) ** 5.0
        a += (1.0 - fc) * g_vis
        b += fc * g_vis
    inv = 1.0 / float(sample_count)
    return a * inv, b * inv


def distribution_ggx(ndoth: float, roughness: float) -> float:
    a = roughness * roughness
    a2 = a * a
    denom = ndoth * ndoth * (a2 - 1.0) + 1.0
    return a2 / max(math.pi * denom * denom, 1e-4)


def bake_sky_faces(env_root: Path, hdr: np.ndarray, size: int, exposure_scale: float) -> None:
    out_dir = env_root / "sky"
    for face in FACE_ORDER:
        pixels: list[tuple[int, int, int, int]] = []
        for y in range(size):
            v = (y + 0.5) / size
            for x in range(size):
                u = (x + 0.5) / size
                direction = cubemap_direction(face, u, v)
                linear = sample_equirect_scaled(hdr, direction, exposure_scale)
                srgb = tuple(linear_to_srgb(channel) for channel in linear)
                pixels.append(tuple(int(round(c * 255.0)) for c in srgb) + (255,))
        write_png_rgba8(out_dir / f"{face}.png", size, size, pixels)


def bake_irradiance(env_root: Path, hdr: np.ndarray, size: int, exposure_scale: float, sample_count: int = 128) -> None:
    out_dir = env_root / "irradiance"
    for face in FACE_ORDER:
        pixels: list[tuple[int, int, int, int]] = []
        for y in range(size):
            v = (y + 0.5) / size
            for x in range(size):
                u = (x + 0.5) / size
                n = cubemap_direction(face, u, v)
                tangent, bitangent = build_tangent_frame(n)
                irradiance = [0.0, 0.0, 0.0]
                weight = 0.0
                for i in range(sample_count):
                    l = transform_tangent_vector(tangent, bitangent, n, hemisphere_sample(hammersley(i, sample_count)))
                    ndotl = max(0.0, l[0] * n[0] + l[1] * n[1] + l[2] * n[2])
                    if ndotl <= 0.0:
                        continue
                    sample = sample_equirect_scaled(hdr, l, exposure_scale)
                    irradiance[0] += sample[0] * ndotl
                    irradiance[1] += sample[1] * ndotl
                    irradiance[2] += sample[2] * ndotl
                    weight += ndotl
                if weight > 1e-4:
                    irradiance = [c / weight for c in irradiance]
                pixels.append((linear_to_byte(irradiance[0]), linear_to_byte(irradiance[1]), linear_to_byte(irradiance[2]), 255))
        write_png_rgba8(out_dir / f"{face}.png", size, size, pixels)


def bake_prefilter_face(hdr: np.ndarray, face: str, size: int, roughness: float, exposure_scale: float, sample_count: int) -> list[tuple[int, int, int, int]]:
    pixels: list[tuple[int, int, int, int]] = []
    for y in range(size):
        v = (y + 0.5) / size
        for x in range(size):
            u = (x + 0.5) / size
            n = cubemap_direction(face, u, v)
            v_dir = n
            prefiltered = [0.0, 0.0, 0.0]
            total = 0.0
            for i in range(sample_count):
                xi = hammersley(i, sample_count)
                h = importance_sample_ggx(xi, roughness, n)
                l = reflect_vector(v_dir, h)
                ndotl = max(0.0, l[0] * n[0] + l[1] * n[1] + l[2] * n[2])
                ndoth = max(0.0, h[0] * n[0] + h[1] * n[1] + h[2] * n[2])
                vdoth = max(0.0, v_dir[0] * h[0] + v_dir[1] * h[1] + v_dir[2] * h[2])
                if ndotl <= 0.0:
                    continue
                d = distribution_ggx(ndoth, roughness)
                pdf = d * ndoth / max(4.0 * vdoth, 1e-4) + 1e-4
                sample = sample_equirect_scaled(hdr, l, exposure_scale)
                weight = ndotl / pdf
                prefiltered[0] += sample[0] * weight
                prefiltered[1] += sample[1] * weight
                prefiltered[2] += sample[2] * weight
                total += weight
            if total > 1e-4:
                prefiltered = [c / total for c in prefiltered]
            else:
                prefiltered = list(sample_equirect_scaled(hdr, n, exposure_scale))
            pixels.append((linear_to_byte(prefiltered[0]), linear_to_byte(prefiltered[1]), linear_to_byte(prefiltered[2]), 255))
    return pixels


def bake_prefilter_mip_chain(env_root: Path, hdr: np.ndarray, base_size: int, mip_count: int, exposure_scale: float, sample_count: int = 128) -> None:
    out_root = env_root / "prefilter"
    for mip in range(mip_count):
        roughness = 0.0 if mip_count <= 1 else float(mip) / float(mip_count - 1)
        size = max(1, base_size >> mip)
        mip_samples = max(16, sample_count // (1 + mip // 2))
        mip_dir = out_root / f"mip{mip:02d}"
        print(f"  prefilter mip{mip:02d} size={size} roughness={roughness:.3f} samples={mip_samples}")
        for face in FACE_ORDER:
            pixels = bake_prefilter_face(hdr, face, size, roughness, exposure_scale, mip_samples)
            write_png_rgba8(mip_dir / f"{face}.png", size, size, pixels)


def bake_prefilter_legacy_flat(env_root: Path, hdr: np.ndarray, size: int, exposure_scale: float, sample_count: int = 128) -> None:
    out_dir = env_root / "prefilter"
    for face in FACE_ORDER:
        pixels = bake_prefilter_face(hdr, face, size, 0.0, exposure_scale, sample_count)
        write_png_rgba8(out_dir / f"{face}.png", size, size, pixels)


def bake_brdf_lut(env_root: Path, size: int = 512, sample_count: int = 128) -> None:
    pixels: list[tuple[int, int, int, int]] = []
    for y in range(size):
        if y % 64 == 0:
            print(f"  BRDF LUT row {y}/{size}")
        roughness = (y + 0.5) / size
        for x in range(size):
            ndotv = max((x + 0.5) / size, 1e-4)
            scale, bias = integrate_brdf(ndotv, roughness, sample_count)
            pixels.append((linear_to_byte(scale), linear_to_byte(bias), 0, 255))
    write_png_rgba8(env_root / "brdf_lut.png", size, size, pixels)


def write_manifest_mip_chain(env_root: Path, hdri_slug: str, hdri_resolution: str, prefilter_mip_count: int, prefilter_base_size: int) -> None:
    manifest = {
        "schemaVersion": 3,
        "name": "default",
        "sourceHdri": {
            "provider": "Poly Haven (CC0)",
            "slug": hdri_slug,
            "resolution": hdri_resolution,
            "url": f"https://polyhaven.com/a/{hdri_slug}",
        },
        "irradiance": "irradiance",
        "prefilter": {
            "layout": "per-mip-faces",
            "baseDir": "prefilter",
            "baseSize": prefilter_base_size,
            "mipCount": prefilter_mip_count,
        },
        "brdfLut": "brdf_lut.png",
        "sky": "sky",
    }
    (env_root / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


def write_manifest_legacy_flat(env_root: Path, hdri_slug: str, hdri_resolution: str) -> None:
    manifest = {
        "schemaVersion": 2,
        "name": "default",
        "sourceHdri": {
            "provider": "Poly Haven (CC0)",
            "slug": hdri_slug,
            "resolution": hdri_resolution,
            "url": f"https://polyhaven.com/a/{hdri_slug}",
        },
        "irradiance": "irradiance",
        "prefilter": "prefilter",
        "brdfLut": "brdf_lut.png",
        "sky": "sky",
    }
    (env_root / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--hdri-slug", default=DEFAULT_HDRI_SLUG, help="Poly Haven HDRI asset slug")
    parser.add_argument("--hdri-resolution", default="1k", choices=("1k", "2k", "4k"))
    parser.add_argument("--sky-size", type=int, default=512)
    parser.add_argument("--irradiance-size", type=int, default=32)
    parser.add_argument("--prefilter-size", type=int, default=128)
    parser.add_argument("--prefilter-mips", type=int, default=8, help="GGX prefilter mip count (per-mip-faces layout)")
    parser.add_argument("--prefilter-legacy-flat", action="store_true", help="Write single-mip flat prefilter/ (schema v2)")
    parser.add_argument("--exposure-scale", type=float, default=None, help="Linear scale before LDR encode; auto when omitted")
    parser.add_argument("--skip-brdf", action="store_true")
    parser.add_argument("--prefilter-only", action="store_true", help="Rebake prefilter + manifest only (reuse cached HDRI)")
    args = parser.parse_args()

    env_root = args.repo_root / "Data" / "Environments" / "default"
    cache_path = env_root / "source" / f"{args.hdri_slug}_{args.hdri_resolution}.hdr"

    print(f"Baking IBL assets under {env_root}")
    download_hdri(args.hdri_slug, args.hdri_resolution, cache_path)
    hdr = load_radiance_hdr(cache_path)
    exposure_scale = args.exposure_scale if args.exposure_scale is not None else compute_exposure_scale(hdr)
    print(f"HDRI shape={hdr.shape[1]}x{hdr.shape[0]} exposureScale={exposure_scale:.4f}")

    if not args.prefilter_only:
        print("Sky cubemap...")
        bake_sky_faces(env_root, hdr, args.sky_size, exposure_scale)
        print("Irradiance cubemap...")
        bake_irradiance(env_root, hdr, args.irradiance_size, exposure_scale)
    print("Prefilter cubemap...")
    if args.prefilter_legacy_flat:
        bake_prefilter_legacy_flat(env_root, hdr, args.prefilter_size, exposure_scale)
        write_manifest_legacy_flat(env_root, args.hdri_slug, args.hdri_resolution)
    else:
        bake_prefilter_mip_chain(env_root, hdr, args.prefilter_size, args.prefilter_mips, exposure_scale)
        write_manifest_mip_chain(env_root, args.hdri_slug, args.hdri_resolution, args.prefilter_mips, args.prefilter_size)
    if not args.skip_brdf:
        print("BRDF LUT...")
        bake_brdf_lut(env_root)
    else:
        print("Skipping BRDF LUT (unchanged).")
    print("Done.")


if __name__ == "__main__":
    main()
