#!/usr/bin/env python3
############################################################################
# tools/preview/compare.py
#
# Put each out/*.png next to the reference of the same name in
# docs/launcher-v2/images: reference | preview | difference, half size,
# written to out/compare/.  Prints the mean difference per image.
############################################################################

import glob
import os

from PIL import Image, ImageChops

HERE = os.path.dirname(os.path.abspath(__file__))
REFS = os.path.join(HERE, "..", "..", "docs", "launcher-v2", "images")
OUT = os.path.join(HERE, "out")


def main():
    os.makedirs(os.path.join(OUT, "compare"), exist_ok=True)

    for ref_path in sorted(glob.glob(os.path.join(REFS, "*.png"))):
        name = os.path.basename(ref_path)
        out_path = os.path.join(OUT, name)
        if not os.path.exists(out_path):
            print("%-28s no preview" % name)
            continue

        ref = Image.open(ref_path).convert("RGB")
        out = Image.open(out_path).convert("RGB").resize(ref.size)
        diff = ImageChops.difference(ref, out)
        mean = sum(diff.convert("L").getdata()) / (ref.size[0] * ref.size[1])

        w, h = ref.size[0] // 2, ref.size[1] // 2
        sheet = Image.new("RGB", (3 * w + 20, h), "white")
        for i, img in enumerate((ref, out, diff)):
            sheet.paste(img.resize((w, h)), (i * (w + 10), 0))

        sheet.save(os.path.join(OUT, "compare", name))
        print("%-28s mean diff %5.1f" % (name, mean))


if __name__ == "__main__":
    main()
