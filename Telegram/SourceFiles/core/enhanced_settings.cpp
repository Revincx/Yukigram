/*
This file is part of 64Gram Desktop,
the unofficial app based on Telegram Desktop.
For license and copyright information please follow this link:
https://github.com/TDesktop-x64/tdesktop/blob/dev/LEGAL
*/
#include "core/enhanced_settings.h"

#include "apiwrap.h"
#include "base/parse_helper.h"
#include "base/qthelp_url.h"
#include "core/chat_enhanced_settings.h"
#include "data/data_histories.h"
#include "data/data_peer_id.h"
#include "data/data_session.h"
#include "history/history.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "settings.h"
#include "ui/toast/toast.h"
#include "window/window_session_controller.h"

#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSet>
#include <QtCore/QTimer>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <type_traits>

namespace EnhancedSettings {
namespace {

constexpr auto kWriteJsonTimeout = crl::time(5000);
constexpr auto kBlocklistPageSize = 100;

QString FromUtf8(std::string_view value) {
	return QString::fromUtf8(value.data(), int(value.size()));
}

Descriptor Bool(
		OptionId id,
		std::string_view storageKey,
		const tr::phrase<> *title,
		std::string_view controlId = {},
		OptionEffects effects = {},
		bool restartRequired = false,
		bool defaultValue = false) {
	return {
		.id = id,
		.storageKey = storageKey,
		.controlId = controlId,
		.title = title,
		.defaultValue = defaultValue,
		.effects = effects,
		.restartRequired = restartRequired,
	};
}

Descriptor Integer(
		OptionId id,
		std::string_view storageKey,
		const tr::phrase<> *title,
		std::string_view controlId,
		IntegerConstraint constraint) {
	return {
		.id = id,
		.storageKey = storageKey,
		.controlId = controlId,
		.title = title,
		.defaultValue = 0,
		.integerConstraint = constraint,
		.normalization = Normalization::IntegerConstraint,
	};
}

const DescriptorList &DescriptorData() {
	static const auto result = DescriptorList{
		Bool(OptionId::ShowMessagesId, "show_messages_id",
			&tr::lng_settings_show_message_id,
			"enhanced/show-message-id", {}, true),
		Descriptor{
			.id = OptionId::ExtraContextMenuOptions,
			.storageKey = "extra_context_menu_options",
			.controlId = "enhanced/extra-context-menu-options",
			.title = &tr::lng_settings_extra_context_menu_options,
			.defaultValue = QList<int>(),
		},
		Bool(OptionId::ShowEmojiButtonAsText, "show_emoji_button_as_text",
			&tr::lng_settings_show_emoji_button_as_text,
			"enhanced/show-emoji-button-as-text", {}, true),
		Bool(OptionId::ShowPhoneNumber, "show_phone_number",
			&tr::lng_settings_show_phone_number, {}, {}, false, true),
		Bool(OptionId::RepeaterReplyToOriginal, "repeater_reply_to_orig_msg",
			&tr::lng_settings_repeater_reply_to_orig_msg,
			"enhanced/repeater-reply-to-original"),
		Bool(OptionId::DisableCloudDraftSync, "disable_cloud_draft_sync",
			&tr::lng_settings_disable_cloud_draft_sync,
			"enhanced/disable-cloud-draft-sync"),
		Bool(OptionId::DisableSyncDraftToCloud, "disable_sync_draft_to_cloud",
			&tr::lng_settings_disable_sync_draft_to_cloud,
			"enhanced/disable-sync-draft-to-cloud"),
		Bool(OptionId::ShowScheduledButton, "show_scheduled_button",
			&tr::lng_settings_show_scheduled_button,
			"enhanced/show-scheduled-button",
			OptionEffect::NotifyShowScheduledButton),
		Bool(OptionId::StereoMode, "stereo_mode",
			&tr::lng_settings_stereo_mode),
		Descriptor{
			.id = OptionId::RadioController,
			.storageKey = "radio_controller",
			.controlId = "enhanced/radio-controller",
			.title = &tr::lng_settings_radio_controller,
			.defaultValue = u"http://localhost:2468"_q,
			.normalization = Normalization::RadioController,
		},
		Bool(OptionId::AutoUnmute, "auto_unmute",
			&tr::lng_settings_auto_unmute, "enhanced/auto-unmute"),
		[] {
				auto result = Integer(
				OptionId::Bitrate,
				"bitrate",
				&tr::lng_bitrate_controller,
				"enhanced/bitrate-controller",
				{ .minimum = 0, .maximum = 7 });
			result.effects = OptionEffect::ShowBitrateHint;
			return result;
		}(),
		Bool(OptionId::HideAllChats, "hide_all_chats",
			&tr::lng_settings_hide_all_chats,
			"enhanced/hide-all-chats", {}, true),
		Bool(OptionId::ReplaceEditButton, "replace_edit_button",
			&tr::lng_settings_replace_edit_button,
			"enhanced/replace-edit-button", OptionEffect::ReloadFiltersMenu),
		Bool(OptionId::HdVideo, "hd_video", &tr::lng_settings_enable_hd_video,
			"enhanced/enable-hd-video", OptionEffect::ShowHdVideoHint),
		Bool(OptionId::SkipToNext, "skip_to_next",
			&tr::lng_settings_skip_message, "enhanced/skip-message"),
		Bool(OptionId::DisableLinkWarning, "disable_link_warning",
			&tr::lng_settings_disable_link_warning,
			"enhanced/disable-link-warning"),
		Descriptor{
			.id = OptionId::LinkPreviewRules,
			.storageKey = "link_preview_replace_rules",
			.controlId = "enhanced/link-preview-replacements",
			.title = &tr::lng_link_preview_rules_title,
			.defaultValue = Core::LinkPreviewRules(),
			.normalization = Normalization::LinkPreviewRules,
		},
		Bool(OptionId::HideBlockedMessages, "blocked_user_spoiler_mode",
			&tr::lng_settings_hide_messages,
			"enhanced/hide-blocked-messages",
			OptionEffect::RefreshBlockedMessages),
		Bool(OptionId::DisablePremiumAnimation, "disable_premium_animation",
			&tr::lng_settings_disable_premium_animation,
			"enhanced/disable-premium-animation"),
		Bool(OptionId::DisableGlobalSearch, "disable_global_search",
			&tr::lng_settings_disable_global_search,
			"enhanced/disable-global-search"),
		Bool(OptionId::ShowMediaMetadata, "show_media_metadata",
			&tr::lng_settings_show_media_metadata,
			"enhanced/show-media-metadata"),
		Bool(OptionId::CommunityChatClick, "community_chat_click",
			&tr::lng_settings_community_chat_click,
			"enhanced/community-chat-click"),
		Bool(OptionId::ShowGroupSenderAvatar, "show_group_sender_avatar",
			&tr::lng_settings_show_group_sender_avatar,
			"enhanced/show-group-sender-avatar"),
		Bool(OptionId::ShowGroupSenderOnlineStatus,
			"show_group_sender_online_status",
			&tr::lng_settings_show_group_sender_online_status,
			"enhanced/show-group-sender-online-status"),
		Bool(OptionId::ShowSeconds, "show_seconds",
			&tr::lng_settings_show_seconds, "enhanced/show-seconds", {}, true),
		[] {
			auto result = Integer(
				OptionId::RichMessagePreviewBlocksLimit,
				"rich_message_preview_max_blocks",
				&tr::lng_settings_rich_message_preview_blocks,
				"enhanced/rich-message-blocks-limit",
				{ .minimum = 5, .maximum = 50, .allowZero = true });
			result.effects = OptionEffect::RefreshRichMessages;
			return result;
		}(),
		Bool(OptionId::ForceShowWebPagePreview, "force_show_webpage_preview",
			&tr::lng_settings_force_show_webpage_preview,
			"enhanced/force-show-webpage-preview",
			OptionEffect::RefreshForceShowWebPagePreview),
		Bool(OptionId::DisableAutoFetchWebPagePreview,
			"disable_auto_fetch_webpage_preview",
			&tr::lng_settings_disable_auto_fetch_webpage_preview,
			"enhanced/disable-auto-fetch-webpage-preview",
			OptionEffect::NotifyDisableAutoFetchWebPagePreview),
		Bool(OptionId::RemoveMediaSpoiler, "remove_media_spoiler",
			&tr::lng_settings_remove_media_spoiler,
			"enhanced/remove-media-spoiler", OptionEffect::RefreshMediaSpoiler),
		Bool(OptionId::HideDeleteForOthersCheckbox,
			"hide-delete-for-others-checkbox",
			&tr::lng_settings_hide_delete_for_others_checkbox,
			"enhanced/hide-delete-for-others-checkbox"),
		[] {
			auto result = Integer(
				OptionId::StickerHeight,
				"sticker_height",
				&tr::lng_settings_sticker_height,
				"enhanced/sticker-height",
				{ .minimum = 64, .maximum = 256, .allowZero = true });
			result.effects = OptionEffect::RefreshStickers;
			return result;
		}(),
		Bool(OptionId::HideCounter, "hide_counter",
			&tr::lng_settings_hide_counter, "enhanced/hide-counter"),
		Bool(OptionId::UseGtApi, "use_gt_api",
			&tr::lng_settings_use_gt_api, "enhanced/use-gt-api"),
		Bool(OptionId::TranslateToTc, "translate_to_tc",
			&tr::lng_settings_translate_to_tc,
			"enhanced/translate-to-tc"),
		Bool(OptionId::HideStories, "hide_stories",
			&tr::lng_settings_hide_stories, "enhanced/hide-stories"),
		Bool(OptionId::ForceMobile, "force_mobile",
			&tr::lng_settings_force_mobile, "enhanced/force-mobile", {}, true),
		Bool(OptionId::ShowSimilarOnJoined, "show_similar_on_joined",
			&tr::lng_settings_show_similar_on_joined,
			"enhanced/show-similar-on-joined", {}, true),
		Bool(OptionId::MoreRightActionComments, "more_right_action_comments",
			&tr::lng_settings_more_right_action_comments,
			"enhanced/more-right-action-comments", {}, true),
		Bool(OptionId::SendCommentAfterForwarding,
			"send_comment_after_forwarding",
			&tr::lng_settings_send_comment_after_forwarding,
			"enhanced/send-comment-after-forwarding"),
		Bool(OptionId::MprisCallHangup, "mpris_call_hangup",
			&tr::lng_settings_mpris_call_hangup,
			"enhanced/mpris-call-hangup"),
		Bool(OptionId::ScreenshotMode, "screenshot_mode",
			&tr::lng_settings_screen_shot_mode),
		Bool(OptionId::AllowScreenshots, "allow_screenshot",
			&tr::lng_settings_allow_screenshots),
		Bool(OptionId::HideStarRatings, "hide_star_ratings",
			&tr::lng_settings_hide_star_ratings,
			"enhanced/hide-star-ratings"),
		Bool(OptionId::ShowPeerId, "show_peer_id",
			&tr::lng_settings_show_peer_id, "enhanced/show-peer-id"),
	};
	return result;
}

size_t Index(OptionId id) {
	const auto result = static_cast<size_t>(id);
	Expects(result < kOptionCount);
	return result;
}

StoredValue Normalize(const Descriptor &descriptor, StoredValue value) {
	if (descriptor.normalization == Normalization::IntegerConstraint) {
		const auto constraint = *descriptor.integerConstraint;
		auto number = std::get<int>(value);
		if (constraint.allowZero && number <= 0) {
			number = 0;
		} else {
			number = std::clamp(
				number,
				constraint.minimum,
				constraint.maximum);
		}
		return number;
	} else if (descriptor.normalization == Normalization::RadioController
		&& std::get<QString>(value).isEmpty()) {
		return descriptor.defaultValue;
	}
	return value;
}

QJsonValue SerializeValue(const StoredValue &value) {
	return std::visit([](const auto &current) -> QJsonValue {
		using Type = std::decay_t<decltype(current)>;
		if constexpr (std::is_same_v<Type, bool>
			|| std::is_same_v<Type, int>
			|| std::is_same_v<Type, QString>) {
			return current;
		} else if constexpr (std::is_same_v<Type, QList<int>>) {
			auto result = QJsonArray();
			for (const auto number : current) {
				result.append(number);
			}
			return result;
		} else {
			return current.toJson();
		}
	}, value);
}

bool IsIntegral(const QJsonValue &value) {
	return value.isDouble()
		&& std::isfinite(value.toDouble())
		&& (std::floor(value.toDouble()) == value.toDouble())
		&& (value.toDouble() >= std::numeric_limits<int>::min())
		&& (value.toDouble() <= std::numeric_limits<int>::max());
}

std::optional<StoredValue> DeserializeValue(
		const Descriptor &descriptor,
		const QJsonValue &value,
		bool strict) {
	const auto index = descriptor.defaultValue.index();
	if (index == StoredValue(false).index()) {
		return value.isBool()
			? std::optional<StoredValue>(value.toBool())
			: std::nullopt;
	} else if (index == StoredValue(0).index()) {
		if (!IsIntegral(value)) {
			return std::nullopt;
		}
		const auto number = value.toInt();
		if (strict && descriptor.integerConstraint) {
			const auto constraint = *descriptor.integerConstraint;
			if ((!constraint.allowZero || number != 0)
				&& (number < constraint.minimum
					|| number > constraint.maximum)) {
				return std::nullopt;
			}
		}
		return Normalize(descriptor, number);
	} else if (index == StoredValue(QString()).index()) {
		return value.isString()
			? std::optional<StoredValue>(Normalize(descriptor, value.toString()))
			: std::nullopt;
	} else if (index == StoredValue(QList<int>()).index()) {
		if (!value.isArray()) {
			return std::nullopt;
		}
		auto result = QList<int>();
		for (const auto &entry : value.toArray()) {
			if (!IsIntegral(entry)) {
				return std::nullopt;
			}
			result.push_back(entry.toInt());
		}
		return result;
	} else if (index == StoredValue(Core::LinkPreviewRules()).index()) {
		if (!value.isArray()) {
			return std::nullopt;
		}
		const auto array = value.toArray();
		if (strict) {
			for (const auto &entry : array) {
				if (!entry.isObject()) {
					return std::nullopt;
				}
				const auto object = entry.toObject();
				const auto pattern = object.value(u"url_pattern"_q);
				const auto domain = object.value(u"replacement_domain"_q);
				if (!pattern.isString()
					|| !domain.isString()
					|| !Core::LinkPreviewRules::ValidPattern(pattern.toString())
					|| Core::LinkPreviewRules::NormalizeDomain(
						domain.toString()).isEmpty()
				) {
					return std::nullopt;
				}
			}
		}
		auto result = Core::LinkPreviewRules();
		result.setRules(Core::LinkPreviewRules::FromJson(array));
		return result;
	}
	Unexpected("Unknown enhanced setting type.");
}

QJsonObject SerializeValues(const std::array<StoredValue, kOptionCount> &values) {
	auto result = QJsonObject();
	for (const auto &descriptor : DescriptorData()) {
		result.insert(
			FromUtf8(descriptor.storageKey),
			SerializeValue(values[Index(descriptor.id)]));
	}
	return result;
}

std::array<StoredValue, kOptionCount> DefaultValues() {
	auto result = std::array<StoredValue, kOptionCount>();
	for (const auto &descriptor : DescriptorData()) {
		result[Index(descriptor.id)] = descriptor.defaultValue;
	}
	return result;
}

QString DefaultFilePath() {
	return cWorkingDir() + u"tdata/enhanced-settings-default.json"_q;
}

QString CustomFilePath() {
	return cWorkingDir() + u"tdata/enhanced-settings-custom.json"_q;
}

QString BlocklistFilePath() {
	return cWorkingDir() + u"tdata/blocklist.json"_q;
}

bool ReadJsonObject(const QString &path, QJsonObject &result) {
	auto file = QFile(path);
	if (!file.open(QIODevice::ReadOnly)) {
		return false;
	}
	auto error = QJsonParseError();
	const auto document = QJsonDocument::fromJson(
		base::parse::stripComments(file.readAll()),
		&error);
	if (error.error != QJsonParseError::NoError || !document.isObject()) {
		return false;
	}
	result = document.object();
	return true;
}

bool WriteJsonObject(
		const QString &path,
		const QByteArray &header,
		const QJsonObject &object) {
	auto file = QFile(path);
	if (!file.open(QIODevice::WriteOnly)) {
		return false;
	}
	file.write(header);
	file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
	return true;
}

class Manager final {
public:
	Manager() : _values(DefaultValues()) {
		_writeTimer.setSingleShot(true);
		QObject::connect(&_writeTimer, &QTimer::timeout, [=] {
			writeCurrentSettings();
		});
	}

	void start() {
		if (_started) {
			return;
		}
		_started = true;
		validateDescriptors();
		writeDefaultFileIfNeeded();
		_values = DefaultValues();

		auto custom = QJsonObject();
		if (ReadJsonObject(CustomFilePath(), custom)) {
			cSetEnhancedFirstRun(false);
			deserializeCurrent(custom);
		} else {
			cSetEnhancedFirstRun(true);
			writeCurrentSettings();
		}
		readBlocklist();
	}

	const StoredValue &get(OptionId id) const {
		return _values[Index(id)];
	}

	bool set(OptionId id, StoredValue value) {
		const auto &descriptor = DescriptorFor(id);
		if (value.index() != descriptor.defaultValue.index()) {
			return false;
		}
		value = Normalize(descriptor, std::move(value));
		auto &current = _values[Index(id)];
		if (current == value) {
			return false;
		}
		current = std::move(value);
		_changes.fire_copy(id);
		if (_started && !_writeTimer.isActive()) {
			_writeTimer.start(kWriteJsonTimeout);
		}
		return true;
	}

	rpl::producer<OptionId> changes() const {
		return _changes.events();
	}

	QString serialize() const {
		return QString::fromUtf8(QJsonDocument(
			SerializeValues(_values)
		).toJson(QJsonDocument::Compact));
	}

	bool deserialize(const QString &json) {
		auto error = QJsonParseError();
		const auto document = QJsonDocument::fromJson(json.toUtf8(), &error);
		if (error.error != QJsonParseError::NoError) {
			LOG(("Enhanced Settings: Error parsing import json: %1 (%2)"
				).arg(error.error
				).arg(error.errorString()));
			return false;
		} else if (!document.isObject()) {
			LOG(("Enhanced Settings: Imported json is not an object."));
			return false;
		}

		auto imported = DefaultValues();
		const auto object = document.object();
		for (auto i = object.begin(); i != object.end(); ++i) {
			const auto found = std::find_if(
				DescriptorData().begin(),
				DescriptorData().end(),
				[&](const Descriptor &descriptor) {
					return FromUtf8(descriptor.storageKey) == i.key();
				});
			if (found == DescriptorData().end()) {
				LOG(("Enhanced Settings: Unknown option "
					"'%1' in import.").arg(i.key()));
				continue;
			}
			auto value = DeserializeValue(*found, i.value(), true);
			if (!value) {
				LOG(("Enhanced Settings: Wrong option value "
					"for '%1' in import.").arg(i.key()));
				return false;
			}
			imported[Index(found->id)] = std::move(*value);
		}

		_writeTimer.stop();
		for (auto i = size_t(); i != kOptionCount; ++i) {
			if (_values[i] != imported[i]) {
				_values[i] = std::move(imported[i]);
				_changes.fire(static_cast<OptionId>(i));
			}
		}
		writeCurrentSettings();
		return true;
	}

	void reset() {
		_writeTimer.stop();
		const auto defaults = DefaultValues();
		for (auto i = size_t(); i != kOptionCount; ++i) {
			if (_values[i] != defaults[i]) {
				_values[i] = defaults[i];
				_changes.fire(static_cast<OptionId>(i));
			}
		}
		writeDefaultFileIfNeeded();
		writeCurrentSettings();
	}

	void finish() {
		if (_writeTimer.isActive()) {
			_writeTimer.stop();
			writeCurrentSettings();
		}
	}

	void readBlocklist() {
		auto file = QFile(BlocklistFilePath());
		if (!file.open(QIODevice::ReadOnly)) {
			cSetBlockList({});
			return;
		}
		const auto document = QJsonDocument::fromJson(file.readAll());
		auto result = QList<int64>();
		for (const auto &entry : document.array()) {
			if (entry.isDouble()) {
				result.push_back(int64(entry.toDouble()));
			}
		}
		cSetBlockList(result);
	}

	void addIdToBlocklist(int64 userId) {
		auto values = cBlockList();
		if (!values.contains(userId)) {
			values.push_back(userId);
			writeBlocklist(values);
		}
	}

	void removeIdFromBlocklist(int64 userId) {
		auto values = cBlockList();
		values.removeAll(userId);
		writeBlocklist(values);
	}

	void refreshBlocklist(
			not_null<Window::SessionController*> controller,
			int offset = 0) {
		if (_blocklistRequestId) {
			return;
		}
		if (!offset) {
			_fetchedBlocklist.clear();
			_blocklistCount = 0;
			Ui::Toast::Show(
				u"Please wait a moment, fetching blocklist..."_q);
		}
		const auto session = &controller->session();
		_blocklistRequestId = session->api().request(MTPcontacts_GetBlocked(
			MTP_flags(0),
			MTP_int(offset),
			MTP_int(kBlocklistPageSize)
		)).done([=](const MTPcontacts_Blocked &result) {
			_blocklistRequestId = 0;
			result.match([&](const MTPDcontacts_blockedSlice &data) {
				_blocklistCount = data.vcount().v;
				appendBlockedUsers(data.vusers().v);
				if (_blocklistCount > _fetchedBlocklist.size()) {
					refreshBlocklist(controller, offset + kBlocklistPageSize);
				} else {
					finishBlocklistRefresh(session);
				}
			}, [&](const MTPDcontacts_blocked &data) {
				appendBlockedUsers(data.vusers().v);
				finishBlocklistRefresh(session);
			});
		}).fail([=] {
			_blocklistRequestId = 0;
		}).send();
	}

private:
	void validateDescriptors() const {
		auto storageKeys = QSet<QString>();
		auto controlIds = QSet<QString>();
		for (auto i = size_t(); i != kOptionCount; ++i) {
			const auto &descriptor = DescriptorData()[i];
			Expects(Index(descriptor.id) == i);
			const auto storageKey = FromUtf8(descriptor.storageKey);
			Expects(!storageKey.isEmpty());
			Expects(descriptor.title != nullptr);
			Expects(!storageKeys.contains(storageKey));
			storageKeys.insert(storageKey);
			const auto controlId = FromUtf8(descriptor.controlId);
			if (!controlId.isEmpty()) {
				Expects(controlId.startsWith(u"enhanced/"_q));
				Expects(!controlIds.contains(controlId));
				controlIds.insert(controlId);
			}
		}
	}

	void deserializeCurrent(const QJsonObject &object) {
		for (auto i = object.begin(); i != object.end(); ++i) {
			const auto found = std::find_if(
				DescriptorData().begin(),
				DescriptorData().end(),
				[&](const Descriptor &descriptor) {
					return FromUtf8(descriptor.storageKey) == i.key();
				});
			if (found == DescriptorData().end()) {
				LOG(("Enhanced Settings: Unknown key %1.").arg(i.key()));
				continue;
			}
			if (auto parsed = DeserializeValue(*found, i.value(), false)) {
				_values[Index(found->id)] = std::move(*parsed);
			}
		}
	}

	void writeDefaultFileIfNeeded() const {
		const auto defaults = SerializeValues(DefaultValues());
		auto current = QJsonObject();
		if (ReadJsonObject(DefaultFilePath(), current) && current == defaults) {
			return;
		}
		WriteJsonObject(DefaultFilePath(), QByteArray(R"HEADER(
// This is a list of default options for 64Gram Desktop
// Please don't modify it, its content is not used in any way
// You can place your own options in the 'enhanced-settings-custom.json' file
)HEADER"), defaults);
	}

	void writeCurrentSettings() const {
		WriteJsonObject(CustomFilePath(), QByteArray(R"HEADER(
// This file was automatically generated from current settings
// It's better to edit it with app closed, so there will be no rewrites
// You should restart app to see changes
)HEADER"), SerializeValues(_values));
	}

	void writeBlocklist(const QList<int64> &values) {
		auto array = QJsonArray();
		for (const auto userId : values) {
			array.push_back(userId);
		}
		auto file = QFile(BlocklistFilePath());
		if (!file.open(QIODevice::WriteOnly)) {
			Ui::Toast::Show(u"Failed to save blocklist."_q);
			return;
		}
		file.write(QJsonDocument(array).toJson(QJsonDocument::Compact));
		cSetBlockList(values);
	}

	template <typename Users>
	void appendBlockedUsers(const Users &users) {
		for (const auto &user : users) {
			_fetchedBlocklist.push_back(
				int64(UserId(user.c_user().vid().v).bare));
		}
	}

	void finishBlocklistRefresh(not_null<Main::Session*> session) {
		writeBlocklist(_fetchedBlocklist);
		session->data().histories().hideBlockedMessages();
	}

	std::array<StoredValue, kOptionCount> _values;
	rpl::event_stream<OptionId> _changes;
	QTimer _writeTimer;
	mtpRequestId _blocklistRequestId = 0;
	QList<int64> _fetchedBlocklist;
	int _blocklistCount = 0;
	bool _started = false;
};

std::unique_ptr<Manager> Data;

Manager &EnsureData() {
	if (!Data) {
		Data = std::make_unique<Manager>();
	}
	return *Data;
}

QByteArray CompactJson(const StoredValue &value) {
	const auto json = SerializeValue(value);
	return json.isArray()
		? QJsonDocument(json.toArray()).toJson(QJsonDocument::Compact)
		: QByteArray();
}

} // namespace

const DescriptorList &Descriptors() {
	return DescriptorData();
}

const Descriptor &DescriptorFor(OptionId id) {
	return DescriptorData()[Index(id)];
}

std::optional<OptionId> OptionByControlId(const QString &controlId) {
	if (controlId.isEmpty()) {
		return std::nullopt;
	}
	for (const auto &descriptor : DescriptorData()) {
		if (FromUtf8(descriptor.controlId) == controlId) {
			return descriptor.id;
		}
	}
	return std::nullopt;
}

QString StorageKey(OptionId id) {
	return FromUtf8(DescriptorFor(id).storageKey);
}

QString ControlId(OptionId id) {
	return FromUtf8(DescriptorFor(id).controlId);
}

QString OptionTitle(OptionId id) {
	const auto title = DescriptorFor(id).title;
	Expects(title != nullptr);
	return (*title)(tr::now);
}

const StoredValue &GetValue(OptionId id) {
	return EnsureData().get(id);
}

bool SetValue(OptionId id, StoredValue value) {
	return EnsureData().set(id, std::move(value));
}

rpl::producer<> Changes(OptionId id) {
	return EnsureData().changes()
		| rpl::filter([=](OptionId changed) { return changed == id; })
		| rpl::map_to(rpl::empty);
}

IntegerConstraint IntegerConstraintFor(Key<int> key) {
	const auto &descriptor = DescriptorFor(key.id);
	Expects(descriptor.integerConstraint.has_value());
	return *descriptor.integerConstraint;
}

bool HasExtraContextMenuOption(ExtraContextMenuOption value) {
	return Get(Option::ExtraContextMenuOptions).contains(int(value));
}

QString DeepLink(not_null<Main::Session*> session, OptionId id) {
	const auto controlId = ControlId(id);
	const auto prefix = u"enhanced/"_q;
	if (controlId.isEmpty()) {
		return {};
	}
	Expects(controlId.startsWith(prefix));
	return session->createInternalLinkFull(
		u"%1/"_q.arg(kEnhancedSettingsRouteChannel.utf16())
			+ controlId.mid(prefix.size()));
}

QString DeepLinkWithCurrentValue(
		not_null<Main::Session*> session,
		OptionId id) {
	const auto link = DeepLink(session, id);
	if (link.isEmpty()) {
		return {};
	}
	const auto &value = GetValue(id);
	if (std::holds_alternative<bool>(value)) {
		return link + u"?value="_q
			+ (std::get<bool>(value) ? u"true"_q : u"false"_q);
	} else if (std::holds_alternative<int>(value)) {
		return link + u"?value="_q + QString::number(std::get<int>(value));
	} else if (std::holds_alternative<QString>(value)) {
		return link + u"?value="_q
			+ qthelp::url_encode(std::get<QString>(value));
	}
	const auto encoded = CompactJson(value).toBase64(
		QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
	return link + u"?value="_q + QString::fromLatin1(encoded)
		+ u"&encoding=base64"_q;
}

QString Serialize() {
	return EnsureData().serialize();
}

bool Deserialize(const QString &json) {
	return EnsureData().deserialize(json);
}

std::optional<PendingValue> ParseSharedValue(
		OptionId id,
		const QMap<QString, QString> &params) {
	const auto value = params.constFind(u"value"_q);
	if (value == params.cend()) {
		return std::nullopt;
	}
	const auto &descriptor = DescriptorFor(id);
	auto json = QJsonValue();
	if (!std::holds_alternative<QList<int>>(descriptor.defaultValue)
		&& !std::holds_alternative<Core::LinkPreviewRules>(
			descriptor.defaultValue)
		&& params.contains(u"encoding"_q)) {
		return std::nullopt;
	}
	if (std::holds_alternative<bool>(descriptor.defaultValue)) {
		if (*value == u"true"_q) {
			json = true;
		} else if (*value == u"false"_q) {
			json = false;
		} else {
			return std::nullopt;
		}
	} else if (std::holds_alternative<int>(descriptor.defaultValue)) {
		auto ok = false;
		const auto number = value->toInt(&ok);
		if (!ok || QString::number(number) != *value) {
			return std::nullopt;
		}
		json = number;
	} else if (std::holds_alternative<QString>(descriptor.defaultValue)) {
		if (params.contains(u"encoding"_q)) {
			return std::nullopt;
		}
		json = *value;
	} else {
		if (params.value(u"encoding"_q) != u"base64"_q) {
			return std::nullopt;
		}
		const auto decoded = QByteArray::fromBase64Encoding(
			value->toLatin1(),
			QByteArray::Base64UrlEncoding
				| QByteArray::AbortOnBase64DecodingErrors);
		if (!decoded) {
			return std::nullopt;
		}
		auto error = QJsonParseError();
		const auto document = QJsonDocument::fromJson(decoded.decoded, &error);
		if (error.error != QJsonParseError::NoError || !document.isArray()) {
			return std::nullopt;
		}
		json = document.array();
	}
	if (auto parsed = DeserializeValue(descriptor, json, true)) {
		return PendingValue{ .id = id, .value = std::move(*parsed) };
	}
	return std::nullopt;
}

bool ApplyOption(
		not_null<Window::SessionController*> controller,
		OptionId id,
		StoredValue value,
		RestartNotification restartNotification) {
	if (!SetValue(id, std::move(value))) {
		return false;
	}
	const auto &descriptor = DescriptorFor(id);
	const auto effects = descriptor.effects;
	if (effects & OptionEffect::RefreshForceShowWebPagePreview) {
		if (Get(Option::ForceShowWebPagePreview)) {
			const auto history = controller->activeChatCurrent().owningHistory();
			if (history) {
				history->refreshForceShowWebPagePreviewViews();
				if (const auto migrated = history->migrateFrom()) {
					migrated->refreshForceShowWebPagePreviewViews();
				}
			}
		}
	}
	if (effects & OptionEffect::NotifyDisableAutoFetchWebPagePreview) {
		NotifyChatFeatureChange(
			nullptr,
			ChatFeature::DisableAutoFetchWebPagePreview);
	}
	if (effects & OptionEffect::RefreshMediaSpoiler) {
		controller->session().data().histories().refreshMediaSpoilerViews();
	}
	if (effects & OptionEffect::RefreshBlockedMessages) {
		if (Get(Option::HideBlockedMessages)) {
			EnsureData().refreshBlocklist(controller);
		} else {
			controller->session().data().histories()
				.restoreBlockedHiddenMessages();
		}
	}
	if (effects & OptionEffect::RefreshRichMessages) {
		controller->session().data().histories().refreshRichMessageViews();
	}
	if (effects & OptionEffect::RefreshStickers) {
		controller->session().data().histories().refreshStickerViews();
	}
	if (effects & OptionEffect::NotifyShowScheduledButton) {
		NotifyChatFeatureChange(nullptr, ChatFeature::ShowScheduledButton);
	}
	if (effects & OptionEffect::ReloadFiltersMenu) {
		controller->reloadFiltersMenu();
	}
	if (effects & OptionEffect::ShowHdVideoHint) {
		Ui::Toast::Show(tr::lng_hd_video_hint(tr::now));
	}
	if (effects & OptionEffect::ShowBitrateHint) {
		Ui::Toast::Show(tr::lng_bitrate_controller_hint(tr::now));
	}
	if (descriptor.restartRequired
		&& restartNotification == RestartNotification::Show) {
		controller->showToast(
			tr::lng_settings_restart_to_apply(tr::now));
	}
	return true;
}

bool BlocklistContains(int64 userId) {
	return cBlockList().contains(userId);
}

void AddIdToBlocklist(int64 userId) {
	EnsureData().addIdToBlocklist(userId);
}

void RemoveIdFromBlocklist(int64 userId) {
	EnsureData().removeIdFromBlocklist(userId);
}

void ReadBlocklist() {
	EnsureData().readBlocklist();
}

void Start() {
	EnsureData().start();
}

void Reset() {
	EnsureData().reset();
}

void Finish() {
	if (Data) {
		Data->finish();
	}
}

} // namespace EnhancedSettings
