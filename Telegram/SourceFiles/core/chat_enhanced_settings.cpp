/*
This file is part of Yurigram Desktop,
the unofficial app based on Telegram Desktop.
For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "core/chat_enhanced_settings.h"

#include "core/enhanced_settings.h"
#include "data/data_chat.h"
#include "data/data_peer.h"
#include "data/data_peer_id.h"
#include "data/data_session.h"
#include "history/history.h"
#include "main/main_session.h"
#include "rpl/event_stream.h"
#include "storage/storage_account.h"

#include <array>
#include <string_view>

namespace EnhancedSettings {
namespace {

constexpr auto kChannelIdOffset = int64(1000000000000);

using ValueChanged = void (*)(not_null<PeerData*>, bool);

struct ChatFeatureDescriptor {
	ChatFeature feature = ChatFeature::Count;
	std::string_view storageKey;
	Key<bool> globalValue;
	ValueChanged valueChanged = nullptr;
};

rpl::event_stream<ChatFeatureChange> ChatFeatureChangeEvents;

template <typename Callback>
void ForEachLoadedHistory(
		not_null<PeerData*> peer,
		const Callback &callback) {
	const auto canonical = peer->migrateToOrMe();
	auto &owner = canonical->owner();
	if (const auto history = owner.historyLoaded(canonical)) {
		callback(history);
	}
	if (const auto migrated = canonical->migrateFrom()) {
		if (const auto history = owner.historyLoaded(migrated)) {
			callback(history);
		}
	}
}

void ForceShowWebPagePreviewValueChanged(
		not_null<PeerData*> peer,
		bool enabled) {
	if (!enabled) {
		return;
	}
	ForEachLoadedHistory(peer, [](not_null<History*> history) {
		history->refreshForceShowWebPagePreviewViews();
	});
}

void HideBlockedMessagesValueChanged(
		not_null<PeerData*> peer,
		bool enabled) {
	ForEachLoadedHistory(peer, [=](not_null<History*> history) {
		if (enabled) {
			history->hideBlockedMessages();
		} else {
			history->restoreBlockedHiddenMessages();
		}
	});
}

void RemoveMediaSpoilerValueChanged(
		not_null<PeerData*> peer,
		bool) {
	ForEachLoadedHistory(peer, [](not_null<History*> history) {
		history->refreshMediaSpoilerViews();
	});
}

constexpr auto kChatFeatureDescriptors = std::array{
	ChatFeatureDescriptor{
		.feature = ChatFeature::ForceShowWebPagePreview,
		.storageKey = "force_show_webpage_preview",
		.globalValue = Option::ForceShowWebPagePreview,
		.valueChanged = ForceShowWebPagePreviewValueChanged,
	},
	ChatFeatureDescriptor{
		.feature = ChatFeature::DisableAutoFetchWebPagePreview,
		.storageKey = "disable_auto_fetch_webpage_preview",
		.globalValue = Option::DisableAutoFetchWebPagePreview,
	},
	ChatFeatureDescriptor{
		.feature = ChatFeature::RemoveMediaSpoiler,
		.storageKey = "remove_media_spoiler",
		.globalValue = Option::RemoveMediaSpoiler,
		.valueChanged = RemoveMediaSpoilerValueChanged,
	},
	ChatFeatureDescriptor{
		.feature = ChatFeature::HideBlockedMessages,
		.storageKey = "hide_blocked_messages",
		.globalValue = Option::HideBlockedMessages,
		.valueChanged = HideBlockedMessagesValueChanged,
	},
	ChatFeatureDescriptor{
		.feature = ChatFeature::ShowScheduledButton,
		.storageKey = "show_scheduled_button",
		.globalValue = Option::ShowScheduledButton,
	},
	ChatFeatureDescriptor{
		.feature = ChatFeature::DisableCloudDraftSync,
		.storageKey = "disable_cloud_draft_sync",
		.globalValue = Option::DisableCloudDraftSync,
	},
	ChatFeatureDescriptor{
		.feature = ChatFeature::DisableSyncDraftToCloud,
		.storageKey = "disable_sync_draft_to_cloud",
		.globalValue = Option::DisableSyncDraftToCloud,
	},
};
static_assert(
	kChatFeatureDescriptors.size()
	== static_cast<std::size_t>(ChatFeature::Count));

const ChatFeatureDescriptor &DescriptorFor(ChatFeature feature) {
	for (const auto &descriptor : kChatFeatureDescriptors) {
		if (descriptor.feature == feature) {
			return descriptor;
		}
	}
	Unexpected("Unknown ChatFeature.");
}

QByteArray StorageKeyFor(
		PeerId peerId,
		const ChatFeatureDescriptor &descriptor) {
	auto result = QByteArray("enhanced.chat.");
	result.append(QByteArray::number(SerializePeerId(peerId)));
	result.append('.');
	result.append(
		descriptor.storageKey.data(),
		static_cast<int>(descriptor.storageKey.size()));
	return result;
}

std::string_view PrefKey(const QByteArray &key) {
	return { key.constData(), static_cast<std::size_t>(key.size()) };
}

ChatFeatureOverride ReadOverride(
		not_null<PeerData*> peer,
		const ChatFeatureDescriptor &descriptor) {
	const auto canonical = peer->migrateToOrMe();
	auto &local = peer->session().local();
	const auto key = StorageKeyFor(canonical->id, descriptor);
	auto stored = local.readPrefOptional<bool>(PrefKey(key));
	if (!stored) {
		if (const auto migrated = canonical->migrateFrom()) {
			const auto migratedKey = StorageKeyFor(migrated->id, descriptor);
			stored = local.readPrefOptional<bool>(PrefKey(migratedKey));
		}
	}
	if (!stored) {
		return ChatFeatureOverride::Default;
	}
	return *stored
		? ChatFeatureOverride::Enabled
		: ChatFeatureOverride::Disabled;
}

} // namespace

OptionId OptionForChatFeature(ChatFeature feature) {
	return DescriptorFor(feature).globalValue.id;
}

std::optional<ChatFeature> ChatFeatureForOption(OptionId id) {
	for (const auto &descriptor : kChatFeatureDescriptors) {
		if (descriptor.globalValue.id == id) {
			return descriptor.feature;
		}
	}
	return std::nullopt;
}

QString ChatFeatureOverrideValue(ChatFeatureOverride value) {
	switch (value) {
	case ChatFeatureOverride::Default:
		return u"default"_q;
	case ChatFeatureOverride::Enabled:
		return u"enabled"_q;
	case ChatFeatureOverride::Disabled:
		return u"disabled"_q;
	}
	Unexpected("Unknown ChatFeatureOverride.");
}

std::optional<ChatFeatureOverride> ParseChatFeatureOverride(
		const QString &value) {
	for (const auto candidate : {
		ChatFeatureOverride::Default,
		ChatFeatureOverride::Enabled,
		ChatFeatureOverride::Disabled,
	}) {
		if (ChatFeatureOverrideValue(candidate) == value) {
			return candidate;
		}
	}
	return std::nullopt;
}

QString ChatPeerIdForLink(PeerId peerId) {
	const auto valid = peerIsUser(peerId)
		|| peerIsChat(peerId)
		|| peerIsChannel(peerId);
	const auto bare = peerId.value & PeerId::kChatTypeMask;
	return (valid && bare) ? QString::number(peerId.value) : QString();
}

PeerId ChatPeerIdFromLink(const QString &value) {
	auto ok = false;
	const auto number = value.toULongLong(&ok);
	if (!ok || !number || QString::number(number) != value) {
		return 0;
	}

	const auto peerId = PeerId(PeerIdHelper(number));
	const auto bare = peerId.value & PeerId::kChatTypeMask;
	return (bare
		&& (peerIsUser(peerId)
			|| peerIsChat(peerId)
			|| peerIsChannel(peerId)))
		? peerId
		: PeerId(0);
}

ChatFeatureOverride GetChatFeatureOverride(
		not_null<PeerData*> peer,
		ChatFeature feature) {
	return ReadOverride(peer, DescriptorFor(feature));
}

bool ResolveChatFeature(
		not_null<PeerData*> peer,
		ChatFeature feature) {
	const auto &descriptor = DescriptorFor(feature);
	switch (ReadOverride(peer, descriptor)) {
	case ChatFeatureOverride::Default:
		return Get(descriptor.globalValue);
	case ChatFeatureOverride::Enabled:
		return true;
	case ChatFeatureOverride::Disabled:
		return false;
	}
	Unexpected("Unknown ChatFeatureOverride.");
}

void SetChatFeatureOverride(
		not_null<PeerData*> peer,
		ChatFeature feature,
		ChatFeatureOverride value) {
	const auto &descriptor = DescriptorFor(feature);
	const auto wasEnabled = ResolveChatFeature(peer, feature);
	const auto canonical = peer->migrateToOrMe();
	const auto key = StorageKeyFor(canonical->id, descriptor);
	auto &local = peer->session().local();
	switch (value) {
	case ChatFeatureOverride::Default:
		local.clearPref(PrefKey(key));
		if (const auto migrated = canonical->migrateFrom()) {
			const auto migratedKey = StorageKeyFor(
				migrated->id,
				descriptor);
			local.clearPref(PrefKey(migratedKey));
		}
		break;
	case ChatFeatureOverride::Enabled:
		local.writePref<bool>(PrefKey(key), true);
		break;
	case ChatFeatureOverride::Disabled:
		local.writePref<bool>(PrefKey(key), false);
		break;
	default:
		Unexpected("Unknown ChatFeatureOverride.");
	}
	const auto enabled = ResolveChatFeature(peer, feature);
	if (wasEnabled != enabled) {
		if (descriptor.valueChanged) {
			descriptor.valueChanged(peer, enabled);
		}
		NotifyChatFeatureChange(peer, feature);
	}
}

void ResetChatFeatureOverrides(not_null<PeerData*> peer) {
	for (const auto &descriptor : kChatFeatureDescriptors) {
		SetChatFeatureOverride(
			peer,
			descriptor.feature,
			ChatFeatureOverride::Default);
	}
}

rpl::producer<ChatFeatureChange> ChatFeatureChanges() {
	return ChatFeatureChangeEvents.events();
}

void NotifyChatFeatureChange(PeerData *peer, ChatFeature feature) {
	ChatFeatureChangeEvents.fire({
		.peer = peer,
		.feature = feature,
	});
}

} // namespace EnhancedSettings
