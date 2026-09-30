import csv
import concurrent.futures
import io
import os
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from urllib.error import URLError
from urllib.request import Request, urlopen
from PIL import Image, ImageOps


SOURCE = Path(__file__).with_name("all_planes.csv")
OUTPUT = Path(__file__).parents[2] / "data" / "planes"
CONVERTER = Path(__file__).parents[3] / "local_lvgl" / "scripts" / "jpg_to_sjpg.py"
USER_AGENT = "KnobbyMTGLifeCounter/1.0 (plane image asset download)"
OUTPUT_IMAGE_SIZE = (360, 360)
JPEG_QUALITY = 90
SPLIT_JPEG_QUALITY = 30


def download_plane(item):
    index, card = item
    uri = card["image_uri"].strip()
    if not uri:
        raise ValueError(f"Missing image_uri for {card['name']}")

    source_path = OUTPUT / f"{index:03d}.jpg"
    if source_path.exists():
        image_data = source_path.read_bytes()
    else:
        request = Request(uri, headers={"User-Agent": USER_AGENT})
        for attempt in range(3):
            try:
                with urlopen(request, timeout=30) as response:
                    image_data = response.read()
                break
            except (URLError, TimeoutError):
                if attempt == 2:
                    raise
                time.sleep(attempt + 1)

    if not image_data.startswith(b"\xff\xd8\xff"):
        raise ValueError(f"URL did not return a JPEG for {card['name']}")

    with Image.open(io.BytesIO(image_data)) as source_image:
        image = ImageOps.exif_transpose(source_image).convert("RGB")
        image = ImageOps.fit(image, OUTPUT_IMAGE_SIZE, method=Image.Resampling.LANCZOS)

        with tempfile.TemporaryDirectory() as temporary_directory:
            working_directory = Path(temporary_directory)
            input_path = working_directory / f"{index:03d}.jpg"
            image.save(input_path, format="JPEG", quality=JPEG_QUALITY, optimize=True)
            converter_environment = os.environ.copy()
            converter_environment["LVGL_SJPG_JPEG_QUALITY"] = str(SPLIT_JPEG_QUALITY)
            subprocess.run(
                [sys.executable, str(CONVERTER), str(input_path)],
                cwd=working_directory,
                env=converter_environment,
                check=True,
                stdout=subprocess.DEVNULL,
            )
            output_path = working_directory / f"{index:03d}.sjpg"
            final_path = OUTPUT / output_path.name
            output_path.replace(final_path)

    return final_path.stat().st_size


with SOURCE.open(newline="", encoding="utf-8-sig") as source:
    cards = list(csv.DictReader(source, strict=True))

if not cards or "image_uri" not in cards[0]:
    raise ValueError("CSV must contain plane cards with an image_uri column")

OUTPUT.mkdir(parents=True, exist_ok=True)
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:
    sizes = list(executor.map(download_plane, enumerate(cards)))

for source_path in OUTPUT.glob("*.jpg"):
    source_path.unlink()

total_size = sum(sizes)
asset_budget = 0x240000
print(f"Downloaded and converted {len(sizes)} planes ({total_size:,} bytes) to {OUTPUT}")
if total_size > asset_budget:
    raise SystemExit(f"Assets exceed the reserved SPIFFS budget of {asset_budget:,} bytes")