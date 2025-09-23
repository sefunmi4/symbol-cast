#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <QImage>
#include <QString>

#include "utils/Logger.hpp"

namespace sc {

class TrocrDecoder {
public:
  TrocrDecoder() = default;
  TrocrDecoder(std::string modelPath, std::string tokenizerPath,
               int expectedSize = 384)
      : m_modelPath(std::move(modelPath)),
        m_tokenizerPath(std::move(tokenizerPath)), m_inputSize(expectedSize) {}

  void setModelPath(std::string path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_modelPath = std::move(path);
    m_moduleLoaded = false;
  }

  void setTokenizerPath(std::string path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tokenizerPath = std::move(path);
    m_tokenizerLoaded = false;
  }

  void setExpectedInputSize(int size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_inputSize = size;
  }

  int expectedInputSize() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_inputSize;
  }

  bool available() const {
    std::lock_guard<std::mutex> lock(m_mutex);
#ifdef SC_ENABLE_TROCR
    return !m_modelPath.empty() && !m_tokenizerPath.empty();
#else
    return false;
#endif
  }

  std::u32string decode(const QImage &glyph) {
#ifdef SC_ENABLE_TROCR
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!ensureLoaded())
      return {};
    if (glyph.isNull())
      return {};

    QImage rgb = glyph;
    if (rgb.format() != QImage::Format_RGBA8888 &&
        rgb.format() != QImage::Format_RGB32 &&
        rgb.format() != QImage::Format_ARGB32 &&
        rgb.format() != QImage::Format_RGB888) {
      rgb = glyph.convertToFormat(QImage::Format_RGBA8888);
    }
    if (rgb.format() == QImage::Format_RGB888) {
      rgb = rgb.convertToFormat(QImage::Format_RGBA8888);
    }

    const int width = rgb.width();
    const int height = rgb.height();
    if (width <= 0 || height <= 0)
      return {};

    const size_t kChannels = 3;
    std::vector<float> buffer(static_cast<size_t>(width) * height * kChannels);
    for (int y = 0; y < height; ++y) {
      const uchar *line = rgb.constScanLine(y);
      for (int x = 0; x < width; ++x) {
        const uchar *pixel = line + x * 4;
        const float r = static_cast<float>(pixel[2]) / 255.f;
        const float g = static_cast<float>(pixel[1]) / 255.f;
        const float b = static_cast<float>(pixel[0]) / 255.f;
        const size_t idx = (static_cast<size_t>(y) * width + x) * kChannels;
        buffer[idx] = (r - 0.5f) / 0.5f;
        buffer[idx + 1] = (g - 0.5f) / 0.5f;
        buffer[idx + 2] = (b - 0.5f) / 0.5f;
      }
    }

    try {
      torch::NoGradGuard guard;
      torch::Tensor input =
          torch::from_blob(buffer.data(), {1, height, width, 3}).clone();
      input = input.permute({0, 3, 1, 2});
      std::vector<torch::jit::IValue> inputs{input};
      torch::Tensor logits = m_module->forward(inputs).toTensor();
      torch::Tensor ids = logits.argmax(-1).to(torch::kCPU).squeeze(0);
      if (ids.dim() == 0)
        return {};
      const auto *data = ids.data_ptr<int64_t>();
      const size_t len = static_cast<size_t>(ids.numel());
      std::vector<uint32_t> tokenIds(len);
      for (size_t i = 0; i < len; ++i)
        tokenIds[i] = static_cast<uint32_t>(data[i]);
      std::string text = m_tokenizer->decode(tokenIds, true);
      QString qtext = QString::fromStdString(text);
      auto u32 = qtext.toUcs4();
      return std::u32string(u32.begin(), u32.end());
    } catch (const c10::Error &err) {
      SC_LOG(sc::LogLevel::Error,
             std::string("TrOCR inference failed: ") + err.what_without_backtrace());
    } catch (const std::exception &ex) {
      SC_LOG(sc::LogLevel::Error, std::string("Tokenizer decode failed: ") +
                                      ex.what());
    }
    return {};
#else
    Q_UNUSED(glyph);
    return {};
#endif
  }

private:
#ifdef SC_ENABLE_TROCR
  bool ensureLoaded() {
    if (!m_moduleLoaded) {
      if (m_modelPath.empty())
        return false;
      try {
        auto module = torch::jit::load(m_modelPath);
        m_module = std::make_shared<torch::jit::Module>(std::move(module));
        m_moduleLoaded = true;
      } catch (const c10::Error &err) {
        SC_LOG(sc::LogLevel::Error,
               std::string("Failed to load TrOCR module: ") +
                   err.what_without_backtrace());
        m_module.reset();
        return false;
      }
    }
    if (!m_tokenizerLoaded) {
      if (m_tokenizerPath.empty())
        return false;
      try {
        auto tokenizer =
            tokenizers::Tokenizer::from_file(m_tokenizerPath.c_str());
        m_tokenizer =
            std::make_shared<tokenizers::Tokenizer>(std::move(tokenizer));
        m_tokenizerLoaded = true;
      } catch (const std::exception &ex) {
        SC_LOG(sc::LogLevel::Error,
               std::string("Failed to load tokenizer: ") + ex.what());
        m_tokenizer.reset();
        return false;
      }
    }
    return m_moduleLoaded && m_tokenizerLoaded;
  }

  std::shared_ptr<torch::jit::Module> m_module;
  std::shared_ptr<tokenizers::Tokenizer> m_tokenizer;
  bool m_moduleLoaded{false};
  bool m_tokenizerLoaded{false};
#else
  bool ensureLoaded() { return false; }
#endif

  std::string m_modelPath;
  std::string m_tokenizerPath;
  int m_inputSize{384};
  mutable std::mutex m_mutex;
};

} // namespace sc
