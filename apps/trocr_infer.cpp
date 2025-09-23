#include "core/recognition/TrocrDecoder.hpp"

#include <QCoreApplication>
#include <QImage>
#include <iostream>

int main(int argc, char **argv) {
#ifdef SC_ENABLE_TROCR
  QCoreApplication app(argc, argv);
  if (argc < 4) {
    std::cerr << "Usage: trocr_infer <module.pt> <tokenizer.json> <image>" << std::endl;
    return 1;
  }

  sc::TrocrDecoder decoder(argv[1], argv[2]);
  QImage image(QString::fromLocal8Bit(argv[3]));
  if (image.isNull()) {
    std::cerr << "Failed to load image: " << argv[3] << std::endl;
    return 1;
  }

  image = image.convertToFormat(QImage::Format_RGBA8888);
  QImage scaled = image.scaled(decoder.expectedInputSize(), decoder.expectedInputSize(),
                               Qt::KeepAspectRatio, Qt::SmoothTransformation);
  std::u32string text = decoder.decode(scaled);
  QString utf8 = QString::fromUcs4(text.data(), static_cast<int>(text.size()));
  std::cout << utf8.toStdString() << std::endl;
  return 0;
#else
  Q_UNUSED(argc);
  Q_UNUSED(argv);
  std::cerr << "TrOCR support is disabled at build time." << std::endl;
  return 1;
#endif
}
