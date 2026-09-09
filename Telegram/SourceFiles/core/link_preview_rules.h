#pragma once

#include <QtCore/QJsonArray>
#include <QtCore/QRegularExpression>
#include <QtCore/QString>

#include <vector>

namespace Core {

struct LinkPreviewRule {
	QString urlPattern;
	QString replacementDomain;

	friend bool operator==(const LinkPreviewRule&, const LinkPreviewRule&)
		= default;
};

class LinkPreviewRules final {
public:
	[[nodiscard]] static bool ValidPattern(const QString &pattern);
	[[nodiscard]] static QString NormalizeDomain(const QString &domain);
	[[nodiscard]] static std::vector<LinkPreviewRule> FromJson(
		const QJsonArray &array);

	void setRules(std::vector<LinkPreviewRule> rules);
	[[nodiscard]] const std::vector<LinkPreviewRule> &rules() const;
	[[nodiscard]] QJsonArray toJson() const;
	[[nodiscard]] QString replaceDomain(const QString &link) const;

private:
	std::vector<LinkPreviewRule> _rules;
	std::vector<QRegularExpression> _patterns;

};

} // namespace Core
