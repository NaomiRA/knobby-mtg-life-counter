import csv
from pathlib import Path


SOURCE = Path(__file__).with_name("all_planes.csv")
OUTPUT = Path(__file__).with_name("planes_data.h")
FIELDS = ("name", "type_line", "oracle_text")


def c_string(value, paragraph_breaks=False):
    if paragraph_breaks:
        value = value.replace("&#xe61d;", "\ue61d")
        value = value.replace("\\r\\n", "\n").replace("\\n", "\n").replace("\\r", "\n")
        value = "\n\n".join(value.splitlines())
    else:
        value = value.replace("\\n", "\n")
    value = value.replace('\\"', '"')
    value = value.replace("\u2014", "-").replace("\u2013", "-")
    encoded = value.encode("utf-8")
    return '"' + "".join(
        "\\\\" if byte == 92 else '\\"' if byte == 34 else
        "\\n" if byte == 10 else chr(byte) if 32 <= byte <= 126 else
        f"\\{byte:03o}"
        for byte in encoded
    ) + '"'


with SOURCE.open(newline="", encoding="utf-8-sig") as source:
    reader = csv.DictReader(source, strict=True)
    if not all(field in reader.fieldnames for field in FIELDS):
        raise ValueError(f"CSV must include: {', '.join(FIELDS)}")
    cards = list(reader)

if not cards or any(not all(card[field] for field in FIELDS) for card in cards):
    raise ValueError("CSV must contain cards with all required fields")

lines = [
    "#ifndef KNOBBY_PLANES_DATA_H",
    "#define KNOBBY_PLANES_DATA_H",
    "",
    "typedef struct {",
    "    const char *name;",
    "    const char *type_line;",
    "    const char *oracle_text;",
    "} plane_card_t;",
    "",
    f"#define PLANE_CARD_COUNT {len(cards)}",
    "static const plane_card_t plane_cards[PLANE_CARD_COUNT] = {",
]
for card in cards:
    lines.append("    {" + ", ".join(c_string(card[field], field == "oracle_text") for field in FIELDS) + "},")
lines.extend(["};", "", "#endif", ""])
OUTPUT.write_text("\n".join(lines), encoding="ascii")
print(f"Generated {len(cards)} planes in {OUTPUT}")