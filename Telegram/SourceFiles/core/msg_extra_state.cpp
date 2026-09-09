/*
This file is part of Yurigram Desktop,
the unofficial app based on Telegram Desktop.
For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "core/msg_extra_state.h"

#include "core/chat_enhanced_settings.h"
#include "data/data_forum_topic.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "rpl/event_stream.h"
#include "settings.h"

#include <algorithm>
#include <map>

namespace MessageExtraState {
namespace {

struct HiddenState {
	uchar sources = 0;
	PeerId blockedPeerId;
};

[[nodiscard]] constexpr auto SourceMask(HiddenSource source) {
	return static_cast<uchar>(source);
}

[[nodiscard]] bool Matches(
		const HiddenState &state,
		std::optional<HiddenSource> source) {
	return !source || (state.sources & SourceMask(*source));
}

using HiddenMessages = std::unordered_map<MsgId, HiddenState>;
using HiddenScopes = std::map<HiddenScope, HiddenMessages>;

std::unordered_map<PeerId, HiddenScopes> hidden;
rpl::event_stream<PeerId> changed;

[[nodiscard]] HiddenScope NaturalScope(not_null<HistoryItem*> item) {
	if (const auto monoforumPeerId = item->sublistPeerId()) {
		return { .monoforumPeerId = monoforumPeerId };
	} else if (const auto topic = item->topic()) {
		return { .repliesRootId = topic->rootId() };
	} else if (const auto repliesRootId = item->replyToTop()) {
		return { .repliesRootId = repliesRootId };
	}
	return {};
}

[[nodiscard]] const HiddenMessages *LookupMessages(
		PeerId peerId,
		HiddenScope scope) {
	const auto peer = hidden.find(peerId);
	if (peer == hidden.end()) {
		return nullptr;
	}
	const auto i = peer->second.find(scope);
	return (i != peer->second.end()) ? &i->second : nullptr;
}

void AppendUnique(std::vector<MsgId> &to, MsgId id) {
	if (ranges::find(to, id) == end(to)) {
		to.push_back(id);
	}
}

template <typename Callback>
void ForEachScope(
		PeerId peerId,
		Callback &&callback) {
	const auto peer = hidden.find(peerId);
	if (peer == hidden.end()) {
		return;
	}
	for (const auto &[scope, messages] : peer->second) {
		callback(scope, messages);
	}
}

template <typename Callback>
void ForEachScope(
		PeerId peerId,
		Callback &&callback,
		int) {
	auto peer = hidden.find(peerId);
	if (peer == hidden.end()) {
		return;
	}
	for (auto i = peer->second.begin(); i != peer->second.end();) {
		auto current = i++;
		callback(current->first, current->second, current, peer);
	}
}

std::vector<MsgId> UnhideFromMessages(
		HiddenMessages &messages,
		const std::vector<MsgId> &ids,
		std::optional<HiddenSource> source) {
	auto changedIds = std::vector<MsgId>();
	changedIds.reserve(ids.size());
	for (const auto messageId : ids) {
		const auto j = messages.find(messageId);
		if (j == messages.end()) {
			continue;
		}
		auto &state = j->second;
		const auto before = state.sources;
		if (!source) {
			state.sources = 0;
			state.blockedPeerId = PeerId();
		} else {
			state.sources &= ~SourceMask(*source);
			if (*source == HiddenSource::BlockedPeer) {
				state.blockedPeerId = PeerId();
			}
		}
		if (before == state.sources) {
			continue;
		}
		changedIds.push_back(messageId);
		if (!state.sources) {
			messages.erase(j);
		}
	}
	return changedIds;
}

} // namespace

bool hide(
		PeerId peerId,
		MsgId messageId,
		HiddenSource source,
		PeerId blockedPeerId,
		std::optional<HiddenScope> scope) {
	auto &messages = hidden[peerId][scope.value_or(HiddenScope())];
	auto &state = messages[messageId];
	const auto mask = SourceMask(source);
	if (state.sources & mask) {
		return false;
	}
	state.sources |= mask;
	if (source == HiddenSource::BlockedPeer) {
		state.blockedPeerId = blockedPeerId;
	}
	changed.fire_copy(peerId);
	return true;
}

bool hide(
		not_null<HistoryItem*> item,
		HiddenSource source,
		std::optional<HiddenScope> scope) {
	const auto blockedPeerId = (source == HiddenSource::BlockedPeer)
		? item->from()->id
		: PeerId();
	return hide(
		item->history()->peer->id,
		item->id,
		source,
		blockedPeerId,
		scope.value_or(NaturalScope(item)));
}

bool isHidden(PeerId peerId, MsgId messageId) {
	auto result = false;
	ForEachScope(peerId, [&](HiddenScope, const HiddenMessages &messages) {
		const auto item = messages.find(messageId);
		result = result
			|| ((item != messages.end()) && (item->second.sources != 0));
	});
	return result;
}

bool isHidden(not_null<HistoryItem*> item) {
	return isHidden(item->history()->peer->id, item->id);
}

int hiddenCount(
		PeerId peerId,
		HiddenScope scope,
		std::optional<HiddenSource> source) {
	const auto messages = LookupMessages(peerId, scope);
	if (!messages) {
		return 0;
	}
	if (!source) {
		return int(messages->size());
	}
	auto result = 0;
	for (const auto &[messageId, state] : *messages) {
		if (Matches(state, source)) {
			++result;
		}
	}
	return result;
}

std::vector<MsgId> hiddenMessageIds(
		PeerId peerId,
		HiddenScope scope,
		std::optional<HiddenSource> source) {
	const auto messages = LookupMessages(peerId, scope);
	if (!messages) {
		return {};
	}
	auto result = std::vector<MsgId>();
	result.reserve(messages->size());
	for (const auto &[messageId, state] : *messages) {
		if (Matches(state, source)) {
			result.push_back(messageId);
		}
	}
	return result;
}

int hiddenCount(
		PeerId peerId,
		std::optional<HiddenSource> source) {
	return int(hiddenMessageIds(peerId, source).size());
}

std::vector<MsgId> hiddenMessageIds(
		PeerId peerId,
		std::optional<HiddenSource> source) {
	auto result = std::vector<MsgId>();
	ForEachScope(peerId, [&](HiddenScope, const HiddenMessages &messages) {
		for (const auto &[messageId, state] : messages) {
			if (Matches(state, source)) {
				AppendUnique(result, messageId);
			}
		}
	});
	return result;
}

std::vector<MsgId> unhide(
		PeerId peerId,
		const std::vector<MsgId> &ids,
		std::optional<HiddenSource> source) {
	if (ids.empty()) {
		return {};
	}
	auto changedIds = std::vector<MsgId>();
	changedIds.reserve(ids.size());
	ForEachScope(peerId, [&](HiddenScope, HiddenMessages &messages, auto current, auto peer) {
		for (const auto messageId : UnhideFromMessages(messages, ids, source)) {
			AppendUnique(changedIds, messageId);
		}
		if (messages.empty()) {
			peer->second.erase(current);
		}
	}, 0);
	const auto peer = hidden.find(peerId);
	if (peer != hidden.end() && peer->second.empty()) {
		hidden.erase(peer);
	}
	if (!changedIds.empty()) {
		changed.fire_copy(peerId);
	}
	return changedIds;
}

std::vector<MsgId> unhideByBlockedPeer(PeerId peerId, PeerId blockedPeerId) {
	auto ids = std::vector<MsgId>();
	ForEachScope(peerId, [&](HiddenScope, const HiddenMessages &messages) {
		for (const auto &[messageId, state] : messages) {
			if ((state.sources & SourceMask(HiddenSource::BlockedPeer))
				&& (state.blockedPeerId == blockedPeerId)) {
				AppendUnique(ids, messageId);
			}
		}
	});
	return unhide(peerId, ids, HiddenSource::BlockedPeer);
}

std::vector<MsgId> unhideAll(
		PeerId peerId,
		HiddenScope scope,
		std::optional<HiddenSource> source) {
	const auto ids = hiddenMessageIds(peerId, scope, source);
	if (ids.empty()) {
		return {};
	}
	auto changedIds = std::vector<MsgId>();
	const auto peer = hidden.find(peerId);
	if (peer == hidden.end()) {
		return {};
	}
	if (const auto i = peer->second.find(scope); i != peer->second.end()) {
		changedIds = UnhideFromMessages(i->second, ids, source);
		if (i->second.empty()) {
			peer->second.erase(i);
		}
		if (peer->second.empty()) {
			hidden.erase(peer);
		}
	}
	if (!changedIds.empty()) {
		changed.fire_copy(peerId);
	}
	return changedIds;
}

std::vector<MsgId> unhideAll(
		PeerId peerId,
		std::optional<HiddenSource> source) {
	return unhide(peerId, hiddenMessageIds(peerId, source), source);
}

std::vector<UnhiddenMessages> unhideAll(HiddenSource source) {
	auto result = std::vector<UnhiddenMessages>();
	result.reserve(hidden.size());
	auto peerIds = std::vector<PeerId>();
	peerIds.reserve(hidden.size());
	for (const auto &[peerId, _] : hidden) {
		peerIds.push_back(peerId);
	}
	for (const auto peerId : peerIds) {
		auto ids = unhideAll(peerId, source);
		if (!ids.empty()) {
			result.push_back({
				.peerId = peerId,
				.ids = std::move(ids),
			});
		}
	}
	return result;
}

void hideMessages(
		not_null<Data::Session*> owner,
		const MessageIdsList &ids,
		HiddenSource source,
		std::optional<HiddenScope> scope) {
	auto expanded = MessageIdsList();
	expanded.reserve(ids.size());
	for (const auto &fullId : ids) {
		if (const auto current = owner->message(fullId)) {
			for (const auto &id : owner->itemOrItsGroup(current)) {
				if (std::find(expanded.begin(), expanded.end(), id)
					== expanded.end()) {
					expanded.push_back(id);
				}
			}
		}
	}
	auto histories = std::vector<not_null<History*>>();
	histories.reserve(expanded.size());
	for (const auto &fullId : expanded) {
		if (const auto current = owner->message(fullId)) {
			const auto history = current->history();
			static_cast<void>(hide(current, source, scope));
			current->destroy();
			if (std::find(histories.begin(), histories.end(), history)
				== histories.end()) {
				histories.push_back(history);
			}
		}
	}
	for (const auto &history : histories) {
		history->requestChatListMessage();
	}
}

bool shouldHideBlockedMessage(not_null<HistoryItem*> item) {
	if (!EnhancedSettings::ResolveChatFeature(
			item->history()->peer,
			EnhancedSettings::ChatFeature::HideBlockedMessages)
		|| !item->isRegular()
		|| item->isService()) {
		return false;
	}
	const auto from = item->from();
	if (EnhancedSettings::BlocklistContains(from->id.value)) {
		return true;
	}
	if (const auto user = from->asUser()) {
		return user->isBlocked();
	}
	return false;
}

rpl::producer<PeerId> changes() {
	return changed.events();
}

} // namespace MessageExtraState
