#!/usr/bin/env python3
"""Convert a Real-ESRGAN SRVGGNetCompact (realesr-animevideov3) .pth to Core ML.

    pip install "torch==2.7.0" coremltools numpy   # Python 3.10-3.12
    python tools/coreml/convert_realesrgan.py realesr-animevideov3.pth

Weights: https://github.com/xinntao/Real-ESRGAN/releases/download/v0.2.5.0/realesr-animevideov3.pth
The output is picked up by video2x automatically on macOS (falls back to ncnn if missing).
"""
import argparse
from pathlib import Path

import coremltools as ct
import torch
import torch.nn as nn
import torch.nn.functional as F


class SRVGGNetCompact(nn.Module):
    def __init__(self, num_feat=64, num_conv=16, upscale=4):
        super().__init__()
        self.upscale = upscale
        self.body = nn.ModuleList([nn.Conv2d(3, num_feat, 3, 1, 1), nn.PReLU(num_feat)])
        for _ in range(num_conv):
            self.body += [nn.Conv2d(num_feat, num_feat, 3, 1, 1), nn.PReLU(num_feat)]
        self.body.append(nn.Conv2d(num_feat, 3 * upscale * upscale, 3, 1, 1))

    def forward(self, x):
        out = x
        for layer in self.body:
            out = layer(out)
        out = F.pixel_shuffle(out, self.upscale)
        out = out + F.interpolate(x, scale_factor=self.upscale, mode="nearest")
        # Image output expects 0-255
        return out.clamp(0, 1) * 255


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("weights", type=Path)
    ap.add_argument("-o", "--output-dir", type=Path, default=Path(__file__).parents[2] / "models" / "realesrgan")
    args = ap.parse_args()

    net = SRVGGNetCompact().eval()
    net.load_state_dict(torch.load(args.weights, map_location="cpu", weights_only=True)["params"])

    # ponytail: flexible shapes run ~1.5x slower than a fixed shape; export per-resolution if that matters
    h, w = 360, 640
    shape = ct.Shape(shape=(1, 3, ct.RangeDim(16, 2160, default=h), ct.RangeDim(16, 3840, default=w)))
    model = ct.convert(
        torch.jit.trace(net, torch.rand(1, 3, h, w)),
        inputs=[ct.ImageType(name="x", shape=shape, scale=1 / 255, color_layout=ct.colorlayout.RGB)],
        outputs=[ct.ImageType(name="y", color_layout=ct.colorlayout.RGB)],
        convert_to="mlprogram",
        compute_precision=ct.precision.FLOAT16,
        compute_units=ct.ComputeUnit.CPU_AND_GPU,
        minimum_deployment_target=ct.target.macOS13,
    )
    out = args.output_dir / "realesr-animevideov3-x4.mlpackage"
    out.parent.mkdir(parents=True, exist_ok=True)
    model.save(str(out))
    print(out)


if __name__ == "__main__":
    main()
