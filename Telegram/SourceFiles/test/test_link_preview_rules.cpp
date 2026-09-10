#include "core/link_preview_rules.h"

#include "base/basic_types.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QTextStream>

int main() {
	auto checks = 0;
	const auto check = [&](bool passed, const char *name) {
		++checks;
		if (!passed) {
			qFatal("Failed: %s", name);
		}
	};
	using Rules = Core::LinkPreviewRules;
	check(!Rules::ValidPattern({}), "empty pattern");
	check(!Rules::ValidPattern(u"  "_q), "blank pattern");
	check(!Rules::ValidPattern(u"["_q), "invalid pattern");
	check(
		Rules::ValidPattern(u"https://old\\.example/.*"_q),
		"valid pattern");
	check(
		Rules::ValidPattern(u"(?i)https://old\\.example/.*"_q),
		"valid pattern with options");
	for (const auto &domain : {
		u""_q,
		u"https://new.example"_q,
		u"new.example/path"_q,
		u"new.example:443"_q,
		u"new.example?x=1"_q,
		u"new.example#part"_q,
		u"user@new.example"_q,
		u"-new.example"_q,
		u"new-.example"_q,
		u"new..example"_q,
		u"new example.com"_q,
		u"new.example\nother.example"_q,
	}) {
		check(Rules::NormalizeDomain(domain).isEmpty(), "invalid domain");
	}
	check(
		Rules::NormalizeDomain(u" NEW.EXAMPLE "_q) == u"new.example"_q,
		"normalize target domain");
	check(
		Rules::NormalizeDomain(u"bücher.example"_q)
			== u"xn--bcher-kva.example"_q,
		"international target domain");
	check(
		Rules::NormalizeDomain(QString(64, 'a') + u".example"_q).isEmpty(),
		"overlong domain label");

	auto rules = Rules();
	const auto checkReplacement = [&](QString input, QString expected) {
		const auto original = input;
		const auto actual = rules.replaceDomain(input);
		if (actual != expected) {
			QTextStream(stderr)
				<< "Expected: " << expected << '\n'
				<< "Actual:   " << actual << Qt::endl;
		}
		check(actual == expected, "domain replacement");
		check(input == original, "source remains unchanged");
	};
	rules.setRules({ { u"old\\.example"_q, u"new.example"_q } });
	checkReplacement(
		u"https://old.example/path"_q,
		u"https://new.example/path"_q);
	rules.setRules({ { u"/selected/"_q, u"new.example"_q } });
	checkReplacement(
		u"https://old.example/selected/path"_q,
		u"https://new.example/selected/path"_q);
	checkReplacement(
		u"https://old.example/other/path"_q,
		u"https://old.example/other/path"_q);
	rules.setRules({ {
		u"(?i)(?:https?://)?(?:[^/@]+@)?"
		u"(?:www\\.)?old\\.example"
		u"(?::\\d+)?(?:/.*)?"_q,
		u"new.example"_q,
	} });
	checkReplacement(
		u"https://old.example:8443/a%2fb/%7E?q=1&q=2+3&x=%2f#part%20one"_q,
		u"https://new.example:8443/a%2fb/%7E?q=1&q=2+3&x=%2f#part%20one"_q);
	checkReplacement(
		u"HTTP://WWW.OLD.EXAMPLE/path"_q,
		u"HTTP://new.example/path"_q);
	checkReplacement(u"old.example/path"_q, u"new.example/path"_q);
	checkReplacement(
		u"old.example:8080/path"_q,
		u"new.example:8080/path"_q);
	checkReplacement(
		u"https://user:pass@old.example/a"_q,
		u"https://user:pass@new.example/a"_q);
	checkReplacement(u"https://old.example"_q, u"https://new.example"_q);
	checkReplacement(
		u"https://unrelated.example/old.example?q=old.example#old.example"_q,
		u"https://new.example/old.example?q=old.example#old.example"_q);
	checkReplacement(u"https://notold.example/"_q, u"https://new.example/"_q);
	checkReplacement(
		u"https://old.example.other/"_q,
		u"https://new.example/"_q);
	for (const auto &link : {
		u"ftp://old.example/a"_q,
		u"tg://old.example/a"_q,
		u"mailto:user@old.example"_q,
		u"user@old.example"_q,
		u"https://old.example:wrong/a"_q,
		u"https://old.example/%xx"_q,
		u"https://[::1]/old.example"_q,
		u"text https://old.example/a"_q,
	}) {
		checkReplacement(link, link);
	}

	rules.setRules({
		{ u"https://.*\\.example/.*"_q, u"first.example"_q },
		{ u"https://old\\.example/.*"_q, u"second.example"_q },
		{ u"https://first\\.example/.*"_q, u"third.example"_q },
	});
	checkReplacement(u"https://old.example/a"_q, u"https://first.example/a"_q);
	const auto json = QJsonDocument(rules.toJson()).toJson();
	auto restored = Rules();
	restored.setRules(Rules::FromJson(QJsonDocument::fromJson(json).array()));
	check(restored.rules() == rules.rules(), "ordered JSON round trip");
	check(
		restored.replaceDomain(u"https://old.example/a"_q)
			== u"https://first.example/a"_q,
		"restored behavior");
	auto malformed = rules.toJson();
	malformed.append(QJsonObject{
		{ u"url_pattern"_q, u"["_q },
		{ u"replacement_domain"_q, u"new.example"_q },
	});
	malformed.append(QJsonObject{
		{ u"url_pattern"_q, u".*"_q },
		{ u"replacement_domain"_q, u"https://new.example"_q },
	});
	malformed.append(42);
	malformed.append(QJsonObject());
	check(Rules::FromJson(malformed) == rules.rules(), "skip malformed rules");
	rules.setRules({});
	checkReplacement(u"https://old.example/a"_q, u"https://old.example/a"_q);
	check(rules.toJson().isEmpty(), "empty rules serialization");
	QTextStream(stdout) << "Passed " << checks << " link preview rule checks.\n";
}
