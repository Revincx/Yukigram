/*
This file is part of Yurigram Desktop,
the unofficial app based on Telegram Desktop.
For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "rpl/producer.h"

class PeerData;

namespace EnhancedSettings {

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
[[nodiscard]] rpl::producer<ChatFeatureChange> ChatFeatureChanges();
void NotifyChatFeatureChange(PeerData *peer, ChatFeature feature);

} // namespace EnhancedSettings
