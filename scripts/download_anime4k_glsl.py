#!/usr/bin/python
# -*- coding: utf-8 -*-
import shutil
from pathlib import Path

import requests

ANIME4K_COMMIT = "master"
GITHUB_GLSL_ROOT = (
    f"https://raw.githubusercontent.com/bloc97/Anime4K/{ANIME4K_COMMIT}/glsl"
)
SHADERS_DIR = Path(__file__).parent.parent / "data"


def download_and_combine_files():

    modes = {
        "ModeA": [
            f"{GITHUB_GLSL_ROOT}/Restore/Anime4K_Clamp_Highlights.glsl",
            f"{GITHUB_GLSL_ROOT}/Restore/Anime4K_Restore_CNN_VL.glsl",
            f"{GITHUB_GLSL_ROOT}/Upscale/Anime4K_Upscale_CNN_x2_VL.glsl",
            f"{GITHUB_GLSL_ROOT}/Upscale/Anime4K_AutoDownscalePre_x2.glsl",
            f"{GITHUB_GLSL_ROOT}/Upscale/Anime4K_AutoDownscalePre_x4.glsl",
            f"{GITHUB_GLSL_ROOT}/Upscale/Anime4K_Upscale_CNN_x2_M.glsl",
        ]
    }

    for mode in modes:
        file_contents = ""
        for file in modes[mode]:
            response = requests.get(file, timeout=5)
            response.raise_for_status()
            file_contents += response.text + "\n"

        with (SHADERS_DIR / Path(f"Anime4K_{mode}.glsl")).open("w") as output_file:
            output_file.write(file_contents)


if __name__ == "__main__":
    # clear shaders directory
    if SHADERS_DIR.exists():
        shutil.rmtree(SHADERS_DIR)
    SHADERS_DIR.mkdir(exist_ok=True)

    # download and combine shaders
    download_and_combine_files()
