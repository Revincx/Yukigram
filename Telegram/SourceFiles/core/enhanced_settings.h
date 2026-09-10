/*
This file is part of 64Gram Desktop,
the unofficial app based on Telegram Desktop.
For license and copyright information please follow this link:
https://github.com/TDesktop-x64/tdesktop/blob/dev/LEGAL
*/
#pragma once

#include "base/flags.h"
#include "core/link_preview_rules.h"
#include "rpl/producer.h"

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <variant>

namespace Window {
class SessionController;
} // namespace Window

namespace tr {
template <typename... Tags>
struct phrase;
} // namespace tr

namespace EnhancedSettings {

enum class OptionId {
	ShowMessagesId,
	ExtraContextMenuOptions,
	ShowEmojiButtonAsText,
	ShowPhoneNumber,
	RepeaterReplyToOriginal,
	DisableCloudDraftSync,
	DisableSyncDraftToCloud,
	ShowScheduledButton,
	StereoMode,
	RadioController,
	AutoUnmute,
	Bitrate,
	HideAllChats,
	ReplaceEditButton,
	HdVideo,
	SkipToNext,
	DisableLinkWarning,
	LinkPreviewRules,
	HideBlockedMessages,
	DisablePremiumAnimation,
	DisableGlobalSearch,
	ShowMediaMetadata,
	CommunityChatClick,
	ShowGroupSenderAvatar,
	ShowGroupSenderOnlineStatus,
	ShowSeconds,
	RichMessagePreviewBlocksLimit,
	ForceShowWebPagePreview,
	DisableAutoFetchWebPagePreview,
	RemoveMediaSpoiler,
	HideDeleteForOthersCheckbox,
	StickerHeight,
	HideCounter,
	UseGtApi,
	TranslateToTc,
	HideStories,
	ForceMobile,
	ShowSimilarOnJoined,
	MoreRightActionComments,
	SendCommentAfterForwarding,
	MprisCallHangup,
	ScreenshotMode,
	AllowScreenshots,
	HideStarRatings,
	ShowPeerId,
	Count,
};

inline constexpr auto kOptionCount = static_cast<std::size_t>(OptionId::Count);

template <typename Value>
struct Key {
	OptionId id = OptionId::Count;
};

namespace Option {
inline constexpr auto ShowMessagesId = Key<bool>{ OptionId::ShowMessagesId };
inline constexpr auto ExtraContextMenuOptions
	= Key<QList<int>>{ OptionId::ExtraContextMenuOptions };
inline constexpr auto ShowEmojiButtonAsText
	= Key<bool>{ OptionId::ShowEmojiButtonAsText };
inline constexpr auto ShowPhoneNumber
	= Key<bool>{ OptionId::ShowPhoneNumber };
inline constexpr auto RepeaterReplyToOriginal
	= Key<bool>{ OptionId::RepeaterReplyToOriginal };
inline constexpr auto DisableCloudDraftSync
	= Key<bool>{ OptionId::DisableCloudDraftSync };
inline constexpr auto DisableSyncDraftToCloud
	= Key<bool>{ OptionId::DisableSyncDraftToCloud };
inline constexpr auto ShowScheduledButton
	= Key<bool>{ OptionId::ShowScheduledButton };
inline constexpr auto StereoMode = Key<bool>{ OptionId::StereoMode };
inline constexpr auto RadioController
	= Key<QString>{ OptionId::RadioController };
inline constexpr auto AutoUnmute = Key<bool>{ OptionId::AutoUnmute };
inline constexpr auto Bitrate = Key<int>{ OptionId::Bitrate };
inline constexpr auto HideAllChats = Key<bool>{ OptionId::HideAllChats };
inline constexpr auto ReplaceEditButton
	= Key<bool>{ OptionId::ReplaceEditButton };
inline constexpr auto HdVideo = Key<bool>{ OptionId::HdVideo };
inline constexpr auto SkipToNext = Key<bool>{ OptionId::SkipToNext };
inline constexpr auto DisableLinkWarning
	= Key<bool>{ OptionId::DisableLinkWarning };
inline constexpr auto LinkPreviewRules
	= Key<Core::LinkPreviewRules>{ OptionId::LinkPreviewRules };
inline constexpr auto HideBlockedMessages
	= Key<bool>{ OptionId::HideBlockedMessages };
inline constexpr auto DisablePremiumAnimation
	= Key<bool>{ OptionId::DisablePremiumAnimation };
inline constexpr auto DisableGlobalSearch
	= Key<bool>{ OptionId::DisableGlobalSearch };
inline constexpr auto ShowMediaMetadata
	= Key<bool>{ OptionId::ShowMediaMetadata };
inline constexpr auto CommunityChatClick
	= Key<bool>{ OptionId::CommunityChatClick };
inline constexpr auto ShowGroupSenderAvatar
	= Key<bool>{ OptionId::ShowGroupSenderAvatar };
inline constexpr auto ShowGroupSenderOnlineStatus
	= Key<bool>{ OptionId::ShowGroupSenderOnlineStatus };
inline constexpr auto ShowSeconds = Key<bool>{ OptionId::ShowSeconds };
inline constexpr auto RichMessagePreviewBlocksLimit
	= Key<int>{ OptionId::RichMessagePreviewBlocksLimit };
inline constexpr auto ForceShowWebPagePreview
	= Key<bool>{ OptionId::ForceShowWebPagePreview };
inline constexpr auto DisableAutoFetchWebPagePreview
	= Key<bool>{ OptionId::DisableAutoFetchWebPagePreview };
inline constexpr auto RemoveMediaSpoiler
	= Key<bool>{ OptionId::RemoveMediaSpoiler };
inline constexpr auto HideDeleteForOthersCheckbox
	= Key<bool>{ OptionId::HideDeleteForOthersCheckbox };
inline constexpr auto StickerHeight = Key<int>{ OptionId::StickerHeight };
inline constexpr auto HideCounter = Key<bool>{ OptionId::HideCounter };
inline constexpr auto UseGtApi = Key<bool>{ OptionId::UseGtApi };
inline constexpr auto TranslateToTc = Key<bool>{ OptionId::TranslateToTc };
inline constexpr auto HideStories = Key<bool>{ OptionId::HideStories };
inline constexpr auto ForceMobile = Key<bool>{ OptionId::ForceMobile };
inline constexpr auto ShowSimilarOnJoined
	= Key<bool>{ OptionId::ShowSimilarOnJoined };
inline constexpr auto MoreRightActionComments
	= Key<bool>{ OptionId::MoreRightActionComments };
inline constexpr auto SendCommentAfterForwarding
	= Key<bool>{ OptionId::SendCommentAfterForwarding };
inline constexpr auto MprisCallHangup
	= Key<bool>{ OptionId::MprisCallHangup };
inline constexpr auto ScreenshotMode = Key<bool>{ OptionId::ScreenshotMode };
inline constexpr auto AllowScreenshots
	= Key<bool>{ OptionId::AllowScreenshots };
inline constexpr auto HideStarRatings
	= Key<bool>{ OptionId::HideStarRatings };
inline constexpr auto ShowPeerId = Key<bool>{ OptionId::ShowPeerId };
} // namespace Option

enum class ExtraContextMenuOption : int {
	Repeater = 1,
	HideMessage = 2,
	ViewAsJson = 3,
	MoreForward = 4,
};

struct IntegerConstraint {
	int minimum = 0;
	int maximum = 0;
	bool allowZero = false;
};

enum class Normalization {
	None,
	IntegerConstraint,
	RadioController,
	LinkPreviewRules,
};

enum class OptionEffect : uint32 {
	RefreshForceShowWebPagePreview = (1U << 0),
	NotifyDisableAutoFetchWebPagePreview = (1U << 1),
	RefreshMediaSpoiler = (1U << 2),
	RefreshBlockedMessages = (1U << 3),
	RefreshRichMessages = (1U << 4),
	RefreshStickers = (1U << 5),
	NotifyShowScheduledButton = (1U << 6),
	ReloadFiltersMenu = (1U << 7),
	ShowHdVideoHint = (1U << 8),
	ShowBitrateHint = (1U << 9),
};
inline constexpr bool is_flag_type(OptionEffect) { return true; }
using OptionEffects = base::flags<OptionEffect>;

using StoredValue = std::variant<
	bool,
	int,
	QString,
	QList<int>,
	Core::LinkPreviewRules>;

struct Descriptor {
	OptionId id = OptionId::Count;
	std::string_view storageKey;
	std::string_view controlId;
	const tr::phrase<> *title = nullptr;
	StoredValue defaultValue;
	std::optional<IntegerConstraint> integerConstraint;
	Normalization normalization = Normalization::None;
	OptionEffects effects;
	bool restartRequired = false;
};

using DescriptorList = std::array<Descriptor, kOptionCount>;

struct PendingValue {
	OptionId id = OptionId::Count;
	StoredValue value;
};

enum class RestartNotification {
	Show,
	Skip,
};

[[nodiscard]] const DescriptorList &Descriptors();
[[nodiscard]] const Descriptor &DescriptorFor(OptionId id);
[[nodiscard]] std::optional<OptionId> OptionByControlId(
	const QString &controlId);
[[nodiscard]] QString StorageKey(OptionId id);
[[nodiscard]] QString ControlId(OptionId id);
[[nodiscard]] QString OptionTitle(OptionId id);

[[nodiscard]] const StoredValue &GetValue(OptionId id);
bool SetValue(OptionId id, StoredValue value);
[[nodiscard]] rpl::producer<> Changes(OptionId id);

template <typename Value>
[[nodiscard]] const Value &Get(Key<Value> key) {
	return std::get<Value>(GetValue(key.id));
}

template <typename Value>
bool Set(Key<Value> key, Value value) {
	return SetValue(key.id, StoredValue(std::move(value)));
}

template <typename Value>
[[nodiscard]] rpl::producer<> Changes(Key<Value> key) {
	return Changes(key.id);
}

template <typename Value>
[[nodiscard]] rpl::producer<Value> Watch(Key<Value> key) {
	return rpl::single(Get(key)) | rpl::then(
		Changes(key) | rpl::map([=] { return Get(key); }));
}

[[nodiscard]] IntegerConstraint IntegerConstraintFor(Key<int> key);
[[nodiscard]] bool HasExtraContextMenuOption(ExtraContextMenuOption value);

[[nodiscard]] QString DeepLink(OptionId id);
[[nodiscard]] QString DeepLinkWithCurrentValue(OptionId id);
[[nodiscard]] QString Serialize();
[[nodiscard]] bool Deserialize(const QString &json);
[[nodiscard]] std::optional<PendingValue> ParseSharedValue(
	OptionId id,
	const QMap<QString, QString> &params);

bool ApplyOption(
	not_null<Window::SessionController*> controller,
	OptionId id,
	StoredValue value,
	RestartNotification restartNotification = RestartNotification::Show);

template <typename Value>
bool ApplyOption(
		not_null<Window::SessionController*> controller,
		Key<Value> key,
		Value value) {
	return ApplyOption(controller, key.id, StoredValue(std::move(value)));
}

[[nodiscard]] bool BlocklistContains(int64 userId);
void AddIdToBlocklist(int64 userId);
void RemoveIdFromBlocklist(int64 userId);
void ReadBlocklist();

void Start();
void Reset();
void Finish();

} // namespace EnhancedSettings
