#pragma once

#include <optional>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QString>
#include <QStringList>
#include <QVector>

namespace sc {

class PluginManager {
public:
  struct Manifest {
    QString id;
    QString name;
    QString description;
    QString version;
    QString entry;
    QString language;
    QString trigger;
    QString expression;
    QString latex;
    QString code;
    QString action;
    QString createdAt;
    QString icon;
    QString filePath;
    QStringList shortcuts;
  };

  bool loadFromDirectory(const QDir &directory) {
    m_directory = directory;
    if (!m_directory.exists())
      m_directory.mkpath(QStringLiteral("."));
    return reload();
  }

  bool reload() {
    if (!m_directory.exists())
      m_directory.mkpath(QStringLiteral("."));
    m_manifests.clear();
    m_indexById.clear();
    m_shortcutIndex.clear();

    const QStringList filters{QStringLiteral("*.json")};
    const QStringList files = m_directory.entryList(filters, QDir::Files);
    for (const QString &file : files)
      loadManifestFile(m_directory.filePath(file));
    return true;
  }

  const QVector<Manifest> &manifests() const { return m_manifests; }

  bool contains(const QString &id) const { return m_indexById.contains(id); }

  std::optional<Manifest> manifestForId(const QString &id) const {
    auto it = m_indexById.constFind(id);
    if (it == m_indexById.constEnd())
      return std::nullopt;
    return m_manifests.value(it.value());
  }

  std::optional<Manifest> manifestForShortcut(const QKeySequence &sequence) const {
    const QString key = normalizeShortcut(sequence);
    auto it = m_shortcutIndex.constFind(key);
    if (it == m_shortcutIndex.constEnd())
      return std::nullopt;
    return m_manifests.value(it.value());
  }

  bool saveManifest(const Manifest &manifest, QString *savedPath = nullptr) {
    if (!m_directory.exists())
      m_directory.mkpath(QStringLiteral("."));

    QString fileName = manifest.filePath;
    if (fileName.isEmpty()) {
      QString base = manifest.id.isEmpty() ? QStringLiteral("plugin") : manifest.id;
      if (!base.endsWith(QStringLiteral(".json")))
        base += QStringLiteral(".json");
      fileName = base;
    } else {
      QFileInfo info(fileName);
      if (info.isAbsolute())
        fileName = info.fileName();
      if (!fileName.endsWith(QStringLiteral(".json")))
        fileName += QStringLiteral(".json");
    }

    const QString path = m_directory.filePath(fileName);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
      return false;

    QJsonObject obj;
    if (!manifest.id.isEmpty())
      obj.insert(QStringLiteral("id"), manifest.id);
    if (!manifest.name.isEmpty())
      obj.insert(QStringLiteral("name"), manifest.name);
    if (!manifest.description.isEmpty())
      obj.insert(QStringLiteral("description"), manifest.description);
    if (!manifest.version.isEmpty())
      obj.insert(QStringLiteral("version"), manifest.version);
    if (!manifest.entry.isEmpty())
      obj.insert(QStringLiteral("entry"), manifest.entry);
    if (!manifest.language.isEmpty())
      obj.insert(QStringLiteral("language"), manifest.language);
    if (!manifest.trigger.isEmpty())
      obj.insert(QStringLiteral("trigger"), manifest.trigger);
    if (!manifest.expression.isEmpty())
      obj.insert(QStringLiteral("expression"), manifest.expression);
    if (!manifest.latex.isEmpty())
      obj.insert(QStringLiteral("latex"), manifest.latex);
    if (!manifest.code.isEmpty())
      obj.insert(QStringLiteral("code"), manifest.code);
    if (!manifest.action.isEmpty())
      obj.insert(QStringLiteral("action"), manifest.action);
    if (!manifest.createdAt.isEmpty())
      obj.insert(QStringLiteral("createdAt"), manifest.createdAt);
    if (!manifest.icon.isEmpty())
      obj.insert(QStringLiteral("icon"), manifest.icon);
    if (!manifest.shortcuts.isEmpty()) {
      QJsonArray shortcutsArray;
      for (const QString &shortcut : manifest.shortcuts)
        shortcutsArray.append(shortcut);
      obj.insert(QStringLiteral("shortcuts"), shortcutsArray);
    }

    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    file.close();
    if (savedPath)
      *savedPath = path;

    return reload();
  }

  QDir directory() const { return m_directory; }

private:
  QString normalizeShortcut(const QKeySequence &sequence) const {
    return sequence.toString(QKeySequence::PortableText);
  }

  static QStringList readShortcutList(const QJsonObject &obj) {
    QStringList shortcuts;
    const QJsonValue value = obj.value(QStringLiteral("shortcuts"));
    if (value.isArray()) {
      const QJsonArray array = value.toArray();
      for (const QJsonValue &entry : array) {
        if (entry.isString())
          shortcuts.push_back(entry.toString());
      }
    } else {
      const QJsonValue single = obj.value(QStringLiteral("shortcut"));
      if (single.isString())
        shortcuts.push_back(single.toString());
    }
    return shortcuts;
  }

  bool loadManifestFile(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
      return false;
    const QByteArray data = file.readAll();
    file.close();

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject())
      return false;
    const QJsonObject obj = doc.object();

    Manifest manifest;
    manifest.filePath = QFileInfo(filePath).fileName();
    manifest.id = obj.value(QStringLiteral("id")).toString();
    if (manifest.id.isEmpty())
      manifest.id = QFileInfo(filePath).baseName();
    manifest.name = obj.value(QStringLiteral("name")).toString(manifest.id);
    manifest.description = obj.value(QStringLiteral("description")).toString();
    manifest.version = obj.value(QStringLiteral("version")).toString();
    manifest.entry = obj.value(QStringLiteral("entry")).toString();
    manifest.language = obj.value(QStringLiteral("language")).toString();
    manifest.trigger = obj.value(QStringLiteral("trigger")).toString();
    manifest.expression = obj.value(QStringLiteral("expression")).toString();
    manifest.latex = obj.value(QStringLiteral("latex")).toString();
    manifest.code = obj.value(QStringLiteral("code")).toString(manifest.entry);
    manifest.action = obj.value(QStringLiteral("action")).toString(QStringLiteral("clipboard"));
    manifest.createdAt = obj.value(QStringLiteral("createdAt")).toString();
    manifest.icon = obj.value(QStringLiteral("icon")).toString();
    manifest.shortcuts = readShortcutList(obj);

    const int index = m_manifests.size();
    m_manifests.push_back(manifest);
    m_indexById.insert(manifest.id, index);
    for (const QString &shortcut : manifest.shortcuts) {
      const QKeySequence sequence(shortcut);
      if (sequence.isEmpty())
        continue;
      m_shortcutIndex.insert(normalizeShortcut(sequence), index);
    }
    return true;
  }

  QDir m_directory;
  QVector<Manifest> m_manifests;
  QHash<QString, int> m_indexById;
  QHash<QString, int> m_shortcutIndex;
};

} // namespace sc

