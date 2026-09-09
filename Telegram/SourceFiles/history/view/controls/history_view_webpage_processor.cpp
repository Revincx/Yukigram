/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "history/view/controls/history_view_webpage_processor.h"

#include "base/unixtime.h"
#include "core/chat_enhanced_settings.h"
#include "core/enhanced_settings.h"
#include "data/data_chat_participant_status.h"
#include "data/data_file_origin.h"
#include "data/data_session.h"
#include "data/data_web_page.h"
#include "history/history.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"

namespace HistoryView::Controls {

WebPageText TitleAndDescriptionFromWebPage(not_null<WebPageData*> d) {
	QString resultTitle, resultDescription;
	const auto document = d->document;
	const auto author = d->author;
	const auto siteName = d->siteName;
	const auto title = d->title;
	const auto description = d->description;
	const auto filenameOrUrl = [&] {
		return ((document && !document->filename().isEmpty())
			? document->filename()
			: d->url);
	};
	const auto authorOrFilename = [&] {
		return (author.isEmpty()
			? filenameOrUrl()
			: author);
	};
	const auto descriptionOrAuthor = [&] {
		return (description.text.isEmpty()
			? authorOrFilename()
			: description.text);
	};
	if (siteName.isEmpty()) {
		if (title.isEmpty()) {
			if (description.text.isEmpty()) {
				resultTitle = author;
				resultDescription = filenameOrUrl();
			} else {
				resultTitle = description.text;
				resultDescription = authorOrFilename();
			}
		} else {
			resultTitle = title;
			resultDescription = descriptionOrAuthor();
		}
	} else {
		resultTitle = siteName;
		resultDescription = title.isEmpty()
			? descriptionOrAuthor()
			: title;
	}
	return { resultTitle, resultDescription };
}

bool DrawWebPageDataPreview(
		QPainter &p,
		not_null<WebPageData*> webpage,
		not_null<PeerData*> context,
		QRect to) {
	const auto document = webpage->document;
	const auto photo = webpage->photo;
	if ((!photo || photo->isNull())
		&& (!document
			|| !document->hasThumbnail()
			|| document->isPatternWallPaper())) {
		return false;
	}

	const auto preview = photo
		? photo->getReplyPreview(Data::FileOrigin(), context, false)
		: document->getReplyPreview(Data::FileOrigin(), context, false);
	if (preview) {
		const auto w = preview->width();
		const auto h = preview->height();
		if (w == h) {
			p.drawPixmap(to.x(), to.y(), preview->pix());
		} else {
			const auto from = (w > h)
				? QRect((w - h) / 2, 0, h, h)
				: QRect(0, (h - w) / 2, w, w);
			p.drawPixmap(to, preview->pix(), from);
		}
	}
	return true;
}

[[nodiscard]] bool ShowWebPagePreview(WebPageData *page) {
	return page && !page->failed;
}

WebPageText ProcessWebPageData(WebPageData *page) {
	auto previewText = TitleAndDescriptionFromWebPage(page);
	if (previewText.title.isEmpty()) {
		if (page->document) {
			previewText.title = tr::lng_attach_file(tr::now);
		} else if (page->photo) {
			previewText.title = tr::lng_attach_photo(tr::now);
		}
	}
	return previewText;
}

WebpageResolver::WebpageResolver(not_null<Main::Session*> session)
: _session(session)
, _api(&session->mtp()) {
}

std::optional<WebPageData*> WebpageResolver::lookup(
		const QString &link) const {
	const auto i = _cache.find(link);
	return (i == end(_cache))
		? std::optional<WebPageData*>()
		: (i->second && !i->second->failed)
		? i->second
		: nullptr;
}

QString WebpageResolver::find(not_null<WebPageData*> page) const {
	for (const auto &[link, cached] : _cache) {
		if (cached == page) {
			return link;
		}
	}
	return QString();
}

void WebpageResolver::request(const QString &link, bool force) {
	if (_requestLink == link && !force) {
		return;
	}
	const auto done = [=](const MTPDmessageMediaWebPage &data) {
		const auto page = _session->data().processWebpage(data.vwebpage());
		if (page->pendingTill > 0
			&& page->pendingTill < base::unixtime::now()) {
			page->pendingTill = 0;
			page->failed = true;
		}
		_cache.emplace(link, page->failed ? nullptr : page.get());
		_resolved.fire_copy(link);
	};
	const auto fail = [=] {
		_cache.emplace(link, nullptr);
		_resolved.fire_copy(link);
	};
	_requestLink = link;
	_requestId = _api.request(
		MTPmessages_GetWebPagePreview(
			MTP_flags(0),
			MTP_string(link),
			MTPVector<MTPMessageEntity>()
	)).done([=](
			const MTPmessages_WebPagePreview &result,
			mtpRequestId requestId) {
		if (_requestId == requestId) {
			_requestId = 0;
		}
		const auto &data = result.data();
		_session->data().processUsers(data.vusers());
		data.vmedia().match([=](const MTPDmessageMediaWebPage &data) {
			done(data);
		}, [&](const auto &d) {
			fail();
		});
	}).fail([=](const MTP::Error &error, mtpRequestId requestId) {
		if (_requestId == requestId) {
			_requestId = 0;
		}
		fail();
	}).send();
}

void WebpageResolver::cancel(const QString &link) {
	if (_requestLink == link) {
		_api.request(base::take(_requestId)).cancel();
		_requestLink = QString();
	}
}

WebpageProcessor::WebpageProcessor(
	not_null<History*> history,
	not_null<Ui::InputField*> field)
: _history(history)
, _resolver(std::make_shared<WebpageResolver>(&history->session()))
, _parser(field)
, _timer([=] {
	if (!ShowWebPagePreview(_data)
		|| _link.isEmpty()
		|| (!_draft.manual && automaticFetchDisabled())) {
		return;
	}
	_resolver->request(_link, true);
}) {
	_history->session().downloaderTaskFinished(
	) | rpl::filter([=] {
		return _data && (_data->document || _data->photo);
	}) | rpl::on_next([=] {
		_repaintRequests.fire({});
	}, _lifetime);

	_history->owner().webPageUpdates(
	) | rpl::filter([=](not_null<WebPageData*> page) {
		return (_data == page.get());
	}) | rpl::on_next([=] {
		_draft.id = _data->id;
		_draft.url = _data->url;
		updateFromData();
	}, _lifetime);

	_parser.list().changes(
	) | rpl::on_next([=](QStringList &&parsed) {
		_parsedLinks = std::move(parsed);
		checkPreview();
	}, _lifetime);

	EnhancedSettings::ChatFeatureChanges(
	) | rpl::filter([=](const EnhancedSettings::ChatFeatureChange &change) {
		return (change.feature
				== EnhancedSettings::ChatFeature::DisableAutoFetchWebPagePreview)
			&& (!change.peer
				|| (change.peer->migrateToOrMe()
					== _history->peer->migrateToOrMe()));
	}) | rpl::on_next([=] {
		checkPreview();
	}, _lifetime);

	EnhancedSettings::Changes(EnhancedSettings::Option::LinkPreviewRules) | rpl::on_next([=] {
		if (!_draft.manual) {
			_links.clear();
			checkPreview();
		}
	}, _lifetime);

	_resolver->resolved() | rpl::on_next([=](QString link) {
		if (_link != link
			|| _draft.removed
			|| (!_draft.manual && automaticFetchDisabled())
			|| (_draft.manual && _draft.url != link)) {
			return;
		}
		_data = _resolver->lookup(link).value_or(nullptr);
		if (_data) {
			_draft.id = _data->id;
			_draft.url = _data->url;
			updateFromData();
		} else {
			_links = QStringList();
			checkPreview();
		}
	}, _lifetime);
}

rpl::producer<> WebpageProcessor::repaintRequests() const {
	return _repaintRequests.events();
}

Data::WebPageDraft WebpageProcessor::draft() const {
	return _draft;
}

Data::WebPageDraft WebpageProcessor::draftForSending() const {
	if (!_draft.manual && automaticFetchDisabled()) {
		return { .removed = true };
	}
	if (_draft.manual || _draft.removed) {
		return _draft;
	}
	if (const auto source = link(); !source.isEmpty() && source != _link) {
		auto result = _draft;
		result.url = _link;
		result.manual = true;
		return result;
	}
	return _draft;
}

std::shared_ptr<WebpageResolver> WebpageProcessor::resolver() const {
	return _resolver;
}

const std::vector<MessageLinkRange> &WebpageProcessor::links() const {
	return _parser.ranges();
}

QString WebpageProcessor::link() const {
	if (!_draft.manual) {
		for (const auto &source : _links) {
			if (EnhancedSettings::Get(EnhancedSettings::Option::LinkPreviewRules).replaceDomain(source)
					== _link) {
				return source;
			}
		}
	}
	return _link;
}

void WebpageProcessor::apply(Data::WebPageDraft draft, bool reparse) {
	const auto was = _link;
	if (draft.removed) {
		_draft = draft;
		_parsedLinks = _parser.list().current();
		if (_parsedLinks.empty()) {
			_draft.removed = false;
		}
		_data = nullptr;
		_links = QStringList();
		_link = QString();
		_parsed = WebpageParsed();
		updateFromData();
	} else if (draft.manual && !draft.url.isEmpty()) {
		_draft = draft;
		_parsedLinks = QStringList();
		_links = QStringList();
		_link = _draft.url;
		const auto cached = _resolver->lookup(_link);
		const auto page = draft.id
			? _history->owner().webpage(draft.id).get()
			: cached.value_or(nullptr);
		const auto valid = page
			&& (page->url == draft.url || (cached && *cached == page));
		if (valid) {
			_data = page;
			_draft.id = _data->id;
			_draft.url = _data->url;
			if (const auto link = _resolver->find(page); !link.isEmpty()) {
				_link = link;
			}
			updateFromData();
		} else {
			_resolver->request(_link);
			return;
		}
	} else if (!draft.manual && !_draft.manual) {
		_draft = draft;
		checkNow(reparse);
	}
	if (_link != was) {
		_resolver->cancel(was);
	}
}

void WebpageProcessor::updateFromData() {
	_timer.cancel();
	auto parsed = WebpageParsed();
	if (ShowWebPagePreview(_data)) {
		if (const auto till = _data->pendingTill) {
			parsed.drawPreview = [](QPainter &p, QRect to) {
				return false;
			};
			parsed.title = tr::lng_preview_loading(tr::now);
			parsed.description = _link;

			const auto timeout = till - base::unixtime::now();
			_timer.callOnce(
				std::max(timeout, 0) * crl::time(1000));
		} else {
			const auto webpage = _data;
			const auto context = _history->peer;
			const auto preview = ProcessWebPageData(_data);
			parsed.title = preview.title;
			parsed.description = preview.description;
			parsed.drawPreview = [=](QPainter &p, QRect to) {
				return DrawWebPageDataPreview(p, webpage, context, to);
			};
		}
	}
	_parsed = std::move(parsed);
	_repaintRequests.fire({});
}

void WebpageProcessor::setDisabled(bool disabled) {
	_parser.setDisabled(disabled);
	if (disabled) {
		apply({ .removed = true });
	} else {
		checkNow(false);
	}
}

void WebpageProcessor::restore() {
	if (!automaticFetchDisabled()) {
		apply({}, true);
		return;
	}
	_parser.parseNow();
	if (_parsedLinks.empty()) {
		apply({}, true);
		return;
	}
	apply({
		.url = _parsedLinks.front(),
		.manual = true,
	}, false);
}

void WebpageProcessor::checkNow(bool force) {
	_parser.parseNow();
	if (force) {
		_link = QString();
		_links = QStringList();
		if (_parsedLinks.isEmpty()) {
			_data = nullptr;
			updateFromData();
			return;
		}
	}
	checkPreview();
}

bool WebpageProcessor::automaticFetchDisabled() const {
	return EnhancedSettings::ResolveChatFeature(
		_history->peer,
		EnhancedSettings::ChatFeature::DisableAutoFetchWebPagePreview);
}

void WebpageProcessor::checkPreview() {
	if (_parsedLinks.empty()) {
		_draft.removed = false;
	}
	if (_draft.removed) {
		return;
	} else if (_history->peer
		&& _history->peer->amRestricted(ChatRestriction::EmbedLinks)) {
		apply({ .removed = true });
		_draft.removed = false;
		return;
	} else if (_draft.manual) {
		return;
	} else if (automaticFetchDisabled()) {
		_resolver->cancel(_link);
		const auto hadPreview = _data
			|| !_link.isEmpty()
			|| !_links.isEmpty()
			|| (_draft != Data::WebPageDraft());
		_data = nullptr;
		_links = QStringList();
		_link = QString();
		_draft = {};
		if (hadPreview) {
			updateFromData();
		}
		return;
	} else if (_links == _parsedLinks) {
		return;
	}
	_links = _parsedLinks;

	auto page = (WebPageData*)nullptr;
	auto chosen = QString();
	for (const auto &source : _links) {
		const auto link = EnhancedSettings::Get(EnhancedSettings::Option::LinkPreviewRules).replaceDomain(source);
		const auto value = _resolver->lookup(link);
		if (!value) {
			chosen = link;
			break;
		} else if (*value) {
			chosen = link;
			page = *value;
			break;
		}
	}
	if (_link != chosen) {
		_resolver->cancel(_link);
		_link = chosen;
		if (!page && !_link.isEmpty()) {
			_resolver->request(_link);
		}
	}
	if (page) {
		_data = page;
		_draft.id = _data->id;
		_draft.url = _data->url;
	} else {
		_data = nullptr;
		_draft = {};
	}
	updateFromData();
}

rpl::producer<WebpageParsed> WebpageProcessor::parsedValue() const {
	return _parsed.value();
}

} // namespace HistoryView::Controls
