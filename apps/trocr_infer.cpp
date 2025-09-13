#include <torch/script.h>
#include <opencv2/opencv.hpp>
#include <tokenizers_cpp/tokenizers.h>
#include <iostream>

int main() {
    // Load TorchScript TrOCR model
    torch::jit::Module trocr = torch::jit::load("trocr_traced.pt");

    // Load and preprocess image
    cv::Mat img = cv::imread("test.png", cv::IMREAD_COLOR);
    cv::cvtColor(img, img, cv::COLOR_BGR2RGB);
    img.convertTo(img, CV_32F, 1.0 / 255.0);
    cv::resize(img, img, cv::Size(384, 384));

    auto tensor = torch::from_blob(img.data, {1, img.rows, img.cols, 3});
    tensor = tensor.permute({0, 3, 1, 2});
    tensor = (tensor - 0.5) / 0.5;

    // Inference
    std::vector<torch::jit::IValue> inputs{tensor};
    torch::Tensor logits = trocr.forward(inputs).toTensor();

    // Greedy decode
    auto ids = logits.argmax(-1);
    auto ids_vec = ids.squeeze(0).to(torch::kCPU).data_ptr<int64_t>();
    size_t len = ids.size(1);

    // Tokenizer
    tokenizers::Tokenizer tokenizer =
        tokenizers::Tokenizer::from_file("trocr_processor/tokenizer.json");
    std::vector<uint32_t> token_ids(ids_vec, ids_vec + len);
    std::string text = tokenizer.decode(token_ids, true);
    std::cout << text << std::endl;
    return 0;
}
