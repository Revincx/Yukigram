#include "core/link_preview_rules.h"

#include "base/basic_types.h"

#include <QtCore/QJsonObject>
#include <QtCore/QUrl>

namespace Core {
namespace {

[[nodiscard]] QRegularExpression UrlPattern(const QString &pattern) {
	return QRegularExpression(pattern);
}

} // namespace

bool LinkPreviewRules::ValidPattern(const QString &pattern) {
	if (pattern.trimmed().isEmpty()) {
		return false;
	}
	return QRegularExpression(pattern).isValid();
}

QString LinkPreviewRules::NormalizeDomain(const QString &domain) {
	const auto ace = QUrl::toAce(domain.trimmed()).toLower();
	static const auto valid = QRegularExpression(
		u"\\A[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?"
		u"(?:\\.[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?)+\\.?\\z"_q);
	const auto result = QString::fromLatin1(ace);
	return (ace.size() <= 253 && valid.match(result).hasMatch())
		? result
		: QString();
}

std::vector<LinkPreviewRule> LinkPreviewRules::FromJson(
		const QJsonArray &array) {
	auto result = std::vector<LinkPreviewRule>();
	for (const auto &value : array) {
		const auto object = value.toObject();
		const auto pattern = object.value(u"url_pattern"_q).toString();
		const auto domain = NormalizeDomain(
			object.value(u"replacement_domain"_q).toString());
		if (ValidPattern(pattern) && !domain.isEmpty()) {
			result.push_back({ pattern, domain });
		}
	}
	return result;
}

void LinkPreviewRules::setRules(std::vector<LinkPreviewRule> rules) {
	_rules.clear();
	_patterns.clear();
	for (auto &rule : rules) {
		auto pattern = UrlPattern(rule.urlPattern);
		rule.replacementDomain = NormalizeDomain(rule.replacementDomain);
		if (rule.urlPattern.trimmed().isEmpty()
			|| !pattern.isValid()
			|| rule.replacementDomain.isEmpty()) {
			continue;
		}
		pattern.optimize();
		_rules.push_back(std::move(rule));
		_patterns.push_back(std::move(pattern));
	}
}

const std::vector<LinkPreviewRule> &LinkPreviewRules::rules() const {
	return _rules;
}

QJsonArray LinkPreviewRules::toJson() const {
	auto result = QJsonArray();
	for (const auto &rule : _rules) {
		result.append(QJsonObject{
			{ u"url_pattern"_q, rule.urlPattern },
			{ u"replacement_domain"_q, rule.replacementDomain },
		});
	}
	return result;
}

QString LinkPreviewRules::replaceDomain(const QString &link) const {
	if (_rules.empty()) {
		return link;
	}
	static const auto schemePattern = QRegularExpression(
		u"\\A([a-z][a-z0-9+.-]*)://"_q,
		QRegularExpression::CaseInsensitiveOption);
	const auto schemeMatch = schemePattern.match(link);
	if (schemeMatch.hasMatch()
		&& schemeMatch.captured(1).compare(
			u"http"_q,
			Qt::CaseInsensitive)
		&& schemeMatch.captured(1).compare(
			u"https"_q,
			Qt::CaseInsensitive)) {
		return link;
	}
	static const auto authorityPattern = QRegularExpression(
		u"\\A(?:(https?)://)?([^/?#]+)"_q,
		QRegularExpression::CaseInsensitiveOption);
	const auto match = authorityPattern.match(link);
	if (!match.hasMatch()) {
		return link;
	}
	const auto explicitScheme = !match.captured(1).isEmpty();
	const auto authority = match.captured(2);
	if (!explicitScheme && authority.contains('@')) {
		return link;
	}
	const auto url = QUrl(
		explicitScheme ? link : u"https://"_q + link,
		QUrl::StrictMode);
	if (!url.isValid() || url.host().isEmpty()) {
		return link;
	}
	const auto start = authority.lastIndexOf('@') + 1;
	const auto colon = authority.indexOf(':', start);
	const auto length = (colon < 0 ? authority.size() : colon) - start;
	if (length <= 0 || authority.at(start) == '[') {
		return link;
	}
	for (auto i = size_t(0); i != _rules.size(); ++i) {
		if (!_patterns[i].match(link).hasMatch()) {
			continue;
		}
		auto result = link;
		result.replace(
			match.capturedStart(2) + start,
			length,
			_rules[i].replacementDomain);
		return QUrl(
			explicitScheme ? result : u"https://"_q + result,
			QUrl::StrictMode).isValid()
			? result
			: link;
	}
	return link;
}

} // namespace Core
