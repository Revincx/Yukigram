/*
This file is part of Yurigram Desktop,
the unofficial app based on Telegram Desktop.
For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "data/data_peer_id.h"
#include "rpl/producer.h"

#include <optional>

class PeerData;

namespace EnhancedSettings {

enum class OptionId;

enum class ChatFeature {
	ForceShowWebPagePreview,
	DisableAutoFetchWebPagePreview,
	RemoveMediaSpoiler,
	HideBlockedMessages,
	ShowScheduledButton,
	DisableCloudDraftSync,
	DisableSyncDraftToCloud,
	Count,
};

enum class ChatFeatureOverride {
	Default,
	Enabled,
	Disabled,
};

struct ChatFeatureChange {
	PeerData *peer = nullptr;
	ChatFeature feature = ChatFeature::Count;
};

[[nodiscard]] OptionId OptionForChatFeature(ChatFeature feature);
[[nodiscard]] std::optional<ChatFeature> ChatFeatureForOption(OptionId id);
[[nodiscard]] QString ChatFeatureOverrideValue(ChatFeatureOverride value);
[[nodiscard]] std::optional<ChatFeatureOverride> ParseChatFeatureOverride(
	const QString &value);
[[nodiscard]] QString ChatPeerIdForLink(PeerId peerId);
[[nodiscard]] PeerId ChatPeerIdFromLink(const QString &value);

[[nodiscard]] ChatFeatureOverride GetChatFeatureOverride(
	not_null<PeerData*> peer,
	ChatFeature feature);
[[nodiscard]] bool ResolveChatFeature(
	not_null<PeerData*> peer,
	ChatFeature feature);
void SetChatFeatureOverride(
	not_null<PeerData*> peer,
	ChatFeature feature,
	ChatFeatureOverride value);
void ResetChatFeatureOverrides(not_null<PeerData*> peer);
[[nodiscard]] rpl::producer<ChatFeatureChange> ChatFeatureChanges();
void NotifyChatFeatureChange(PeerData *peer, ChatFeature feature);

} // namespace EnhancedSettings
