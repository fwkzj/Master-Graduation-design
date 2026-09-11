import sys
from pathlib import Path

import pypdfium2 as pdfium


def main(input_pdf: str, output_dir: str, scale: float = 2.0) -> None:
    source = Path(input_pdf)
    target = Path(output_dir)
    target.mkdir(parents=True, exist_ok=True)
    pdf = pdfium.PdfDocument(source)
    for index in range(len(pdf)):
        page = pdf[index]
        bitmap = page.render(scale=scale)
        bitmap.to_pil().save(target / f"page-{index + 1}.png")
        bitmap.close()
        page.close()


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], float(sys.argv[3]) if len(sys.argv) > 3 else 2.0)
