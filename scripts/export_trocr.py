#!/usr/bin/env python3
"""Export a TrOCR model to TorchScript for C++ inference."""
from transformers import TrOCRProcessor, VisionEncoderDecoderModel
import torch
from PIL import Image


def main() -> None:
    model_name = "microsoft/trocr-base-stage1"
    processor = TrOCRProcessor.from_pretrained(model_name)
    model = VisionEncoderDecoderModel.from_pretrained(model_name).eval()

    # Use a dummy image to trace the model
    image = Image.open("dummy.png").convert("RGB")
    pixel_values = processor(images=image, return_tensors="pt").pixel_values

    traced = torch.jit.trace(model, pixel_values)
    traced.save("trocr_traced.pt")

    # Save tokenizer/processor for use in C++
    processor.save_pretrained("./trocr_processor")


if __name__ == "__main__":
    main()
