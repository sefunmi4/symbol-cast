#pragma once

#include <algorithm>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QSet>

namespace sc {

class SymbolicParser {
public:
  struct Result {
    QString expression;
    QString normalized;
    QString latex;
    QString code;
    QStringList variables;
    QStringList tokens;
    bool valid{false};
  };

  Result parse(const QString &input) const {
    Result result;
    result.expression = input;
    QString normalized = input;
    normalized.replace(QRegularExpression(QStringLiteral("\\s+")), QString());
    result.normalized = normalized;
    if (normalized.isEmpty())
      return result;

    QRegularExpression tokenRegex(
        QStringLiteral("([A-Za-z_]+\\w*|\\d+|==|!=|<=|>=|[=+\\-*/^()])"));
    QStringList tokens;
    QRegularExpressionMatchIterator it = tokenRegex.globalMatch(normalized);
    while (it.hasNext()) {
      const QRegularExpressionMatch match = it.next();
      QString token = match.captured(1);
      if (!token.isEmpty())
        tokens.push_back(token);
    }
    if (tokens.isEmpty())
      tokens.push_back(normalized);
    result.tokens = tokens;

    int equalsIndex = tokens.indexOf(QStringLiteral("="));
    QStringList rhsTokens = equalsIndex >= 0 ? tokens.mid(equalsIndex + 1) : tokens;
    result.variables = extractVariables(rhsTokens);

    result.latex = tokensToLatex(tokens);
    result.code = tokensToCode(tokens, rhsTokens, result.variables);
    result.valid = true;
    return result;
  }

private:
  static QString tokensToLatex(const QStringList &tokens) {
    QString latex;
    for (int i = 0; i < tokens.size(); ++i) {
      const QString &token = tokens.at(i);
      if (token == QStringLiteral("*")) {
        latex += QStringLiteral("\\cdot ");
      } else if (token == QStringLiteral("/")) {
        latex += QStringLiteral("\\div ");
      } else if (token == QStringLiteral("^")) {
        if (i + 1 < tokens.size()) {
          latex += QStringLiteral("^{");
          latex += tokens.at(i + 1);
          latex += QLatin1Char('}');
          ++i;
        }
      } else {
        latex += token;
      }
    }
    return latex.trimmed();
  }

  static QString tokensToCode(const QStringList &tokens, const QStringList &rhsTokens,
                              const QStringList &variables) {
    QStringList working = tokens;
    for (QString &token : working) {
      if (token == QStringLiteral("^"))
        token = QStringLiteral("**");
    }
    QString expression = working.join(QString());

    if (!rhsTokens.isEmpty() && working.contains(QStringLiteral("="))) {
      QString rhsExpression;
      for (const QString &token : rhsTokens) {
        if (token == QStringLiteral("^"))
          rhsExpression += QStringLiteral("**");
        else
          rhsExpression += token;
      }
      if (!variables.isEmpty()) {
        QStringList sortedVars = variables;
        std::sort(sortedVars.begin(), sortedVars.end());
        return QStringLiteral("lambda %1: %2")
            .arg(sortedVars.join(QStringLiteral(", ")))
            .arg(rhsExpression);
      }
      return rhsExpression;
    }
    return expression;
  }

  static QStringList extractVariables(const QStringList &tokens) {
    QSet<QString> vars;
    QRegularExpression varRegex(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
    for (const QString &token : tokens) {
      if (varRegex.match(token).hasMatch())
        vars.insert(token);
    }
    QStringList list = vars.values();
    std::sort(list.begin(), list.end());
    return list;
  }
};

} // namespace sc

