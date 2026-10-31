import csv
from pathlib import Path


SOURCE = Path(__file__).with_name("bounties.csv")
OUTPUT = Path(__file__).with_name("bounties_data.h")
FIELDS = ("name", "oracle_text", "image_uri")


def c_string(value):
    value = value.replace("\u00a0", " ").replace("\u2014", "-").replace("\u2013", "-")
    return '"' + "".join(
        "\\\\" if byte == 92 else '\\"' if byte == 34 else
        "\\n" if byte == 10 else chr(byte) if 32 <= byte <= 126 else
        f"\\{byte:03o}"
        for byte in value.encode("utf-8")
    ) + '"'


with SOURCE.open(newline="", encoding="utf-8-sig") as source:
    reader = csv.DictReader(source, strict=True)
    if not reader.fieldnames or not all(field in reader.fieldnames for field in FIELDS):
        raise ValueError(f"CSV must include: {', '.join(FIELDS)}")
    cards = list(reader)

if not cards or any(not all(card.get(field) for field in FIELDS) for card in cards):
    raise ValueError("CSV must contain bounties with all required fields")

lines = [
    "#ifndef KNOBBY_BOUNTIES_DATA_H",
    "#define KNOBBY_BOUNTIES_DATA_H",
    "",
    "typedef struct {",
    "    const char *name;",
    "    const char *oracle_text;",
    "} bounty_card_t;",
    "",
    f"#define BOUNTY_CARD_COUNT {len(cards)}",
    "static const bounty_card_t bounty_cards[BOUNTY_CARD_COUNT] = {",
]
for card in cards:
    lines.append("    {" + ", ".join(c_string(card[field]) for field in FIELDS[:2]) + "},")
lines.extend(["};", "", "#endif", ""])
OUTPUT.write_text("\n".join(lines), encoding="ascii")
print(f"Generated {len(cards)} bounties in {OUTPUT}")