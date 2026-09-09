/*
This file is part of 64Gram Desktop,
the unofficial app based on Telegram Desktop.
For license and copyright information please follow this link:
https://github.com/TDesktop-x64/tdesktop/blob/dev/LEGAL
*/
#include "core/enhanced_settings.h"

#include "mainwindow.h"
#include "mainwidget.h"
#include "window/window_controller.h"
#include "core/application.h"
#include "base/parse_helper.h"
#include "facades.h"
#include "rpl/variable.h"
#include "ui/widgets/fields/input_field.h"
#include "lang/lang_cloud_manager.h"
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonValue>

#include <algorithm>

namespace EnhancedSettings {
	namespace {

		constexpr auto kWriteJsonTimeout = crl::time(5000);
		constexpr auto kRichMessagePreviewBlocksLimitKey
			= "rich_message_preview_max_blocks";
		constexpr auto kForceShowWebPagePreviewKey
			= "force_show_webpage_preview";
		constexpr auto kStickerHeightKey = "sticker_height";
		constexpr auto kAllowScreenshotsKey = "allow_screenshot";
		constexpr auto kShowGroupSenderOnlineStatusKey
			= "show_group_sender_online_status";
		constexpr auto kHideDeleteForOthersCheckboxKey
			= "hide-delete-for-others-checkbox";
		rpl::variable<bool> allowScreenshotsState = false;
		rpl::variable<bool> showGroupSenderOnlineStatusState = false;

		[[nodiscard]] int NormalizeRichMessagePreviewBlocksLimit(int limit) {
			if (limit <= 0) {
				return 0;
			}
			return std::clamp(
				limit,
				kRichMessagePreviewBlocksLimitMin,
				kRichMessagePreviewBlocksLimitMax);
		}

		[[nodiscard]] int NormalizeStickerHeight(int height) {
			if (height <= 0) {
				return 0;
			}
			return std::clamp(height, kStickerHeightMin, kStickerHeightMax);
		}

		QString DefaultFilePath() {
			return cWorkingDir() + qsl("tdata/enhanced-settings-default.json");
		}

		QString CustomFilePath() {
			return cWorkingDir() + qsl("tdata/enhanced-settings-custom.json");
		}

		bool DefaultFileIsValid() {
			QFile file(DefaultFilePath());
			if (!file.open(QIODevice::ReadOnly)) {
				return false;
			}
			auto error = QJsonParseError{0, QJsonParseError::NoError};
			const auto document = QJsonDocument::fromJson(
					base::parse::stripComments(file.readAll()),
					&error);
			file.close();

			if (error.error != QJsonParseError::NoError || !document.isObject()) {
				return false;
			}
			const auto settings = document.object();

			return true;
		}

		void WriteDefaultCustomFile() {
			const auto path = CustomFilePath();
			auto input = QFile(":/misc/default_enhanced-settings-custom.json");
			auto output = QFile(path);
			if (input.open(QIODevice::ReadOnly) && output.open(QIODevice::WriteOnly)) {
				output.write(input.readAll());
			}
		}

		bool ReadOption(QJsonObject obj, QString key, std::function<void(QJsonValue)> callback) {
			const auto it = obj.constFind(key);
			if (it == obj.constEnd()) {
				return false;
			}
			callback(*it);
			return true;
		}

		bool ReadStringOption(QJsonObject obj, QString key, std::function<void(QString)> callback) {
			auto readResult = false;
			auto readValueResult = ReadOption(obj, key, [&](QJsonValue v) {
				if (v.isString()) {
					callback(v.toString());
					readResult = true;
				}
			});
			return (readValueResult && readResult);
		}

		bool ReadIntOption(QJsonObject obj, QString key, std::function<void(int)> callback) {
			auto readResult = false;
			auto readValueResult = ReadOption(obj, key, [&](QJsonValue v) {
				if (v.isDouble()) {
					callback(v.toInt());
					readResult = true;
				}
			});
			return (readValueResult && readResult);
		}

		std::unique_ptr<Manager> Data;

	} // namespace

	int RichMessagePreviewBlocksLimit() {
		return NormalizeRichMessagePreviewBlocksLimit(
			GetEnhancedInt(kRichMessagePreviewBlocksLimitKey));
	}

	void SetRichMessagePreviewBlocksLimit(int limit) {
		SetEnhancedValue(
			kRichMessagePreviewBlocksLimitKey,
			NormalizeRichMessagePreviewBlocksLimit(limit));
	}

	int StickerHeight() {
		return NormalizeStickerHeight(GetEnhancedInt(kStickerHeightKey));
	}

	void SetStickerHeight(int height) {
		SetEnhancedValue(kStickerHeightKey, NormalizeStickerHeight(height));
	}

	bool AllowScreenshots() {
		return allowScreenshotsState.current();
	}

	rpl::producer<bool> AllowScreenshotsValue() {
		return allowScreenshotsState.value();
	}

	void SetAllowScreenshots(bool allow) {
		SetEnhancedValue(kAllowScreenshotsKey, allow);
		allowScreenshotsState = allow;
	}

	bool ShowGroupSenderOnlineStatus() {
		return showGroupSenderOnlineStatusState.current();
	}

	rpl::producer<bool> ShowGroupSenderOnlineStatusValue() {
		return showGroupSenderOnlineStatusState.value();
	}

	void SetShowGroupSenderOnlineStatus(bool show) {
		SetEnhancedValue(kShowGroupSenderOnlineStatusKey, show);
		showGroupSenderOnlineStatusState = show;
	}

	Manager::Manager() {
		_jsonWriteTimer.setSingleShot(true);
		connect(&_jsonWriteTimer, SIGNAL(timeout()), this, SLOT(writeTimeout()));
	}

	void Manager::fill() {
		if (!DefaultFileIsValid()) {
			writeDefaultFile();
		}
		SetEnhancedValue(kHideDeleteForOthersCheckboxKey, false);
		if (!readCustomFile()) {
			WriteDefaultCustomFile();
		}
		SetAllowScreenshots(GetEnhancedBool(kAllowScreenshotsKey));
		SetShowGroupSenderOnlineStatus(
			GetEnhancedBool(kShowGroupSenderOnlineStatusKey));
		readBlocklist();
	}

	void Manager::reset() {
		writing();
		cSetEnhancedOptions({});
		writeDefaultFile();
		SetAllowScreenshots(GetEnhancedBool(kAllowScreenshotsKey));
		SetShowGroupSenderOnlineStatus(
			GetEnhancedBool(kShowGroupSenderOnlineStatusKey));
		writeCurrentSettings();
	}

	void Manager::write(bool force) {
		if (force && _jsonWriteTimer.isActive()) {
			_jsonWriteTimer.stop();
			writeTimeout();
		} else if (!force && !_jsonWriteTimer.isActive()) {
			_jsonWriteTimer.start(kWriteJsonTimeout);
		}
	}

	bool Manager::readCustomFile() {
		QFile file(CustomFilePath());
		if (!file.exists()) {
			cSetEnhancedFirstRun(true);
			return false;
		}
		cSetEnhancedFirstRun(false);
		if (!file.open(QIODevice::ReadOnly)) {
			return true;
		}
		auto error = QJsonParseError{0, QJsonParseError::NoError};
		const auto document = QJsonDocument::fromJson(
				base::parse::stripComments(file.readAll()),
				&error);
		file.close();

		if (error.error != QJsonParseError::NoError) {
			return true;
		} else if (!document.isObject()) {
			return true;
		}
		const auto settings = document.object();

		if (settings.isEmpty()) {
			return true;
		}

		loadSettings(settings);

		ReadOption(settings, "bitrate", [&](auto v) {
			if (v.isDouble()) {
				int value = v.toInt();
				if (value < 0) {
					gEnhancedOptions.insert("bitrate", 0);
				} else if (value > 7) {
					gEnhancedOptions.insert("bitrate", 7);
				} else {
					gEnhancedOptions.insert("bitrate", value);
				}
			}
		});

		ReadIntOption(
			settings,
			kRichMessagePreviewBlocksLimitKey,
			SetRichMessagePreviewBlocksLimit);
		ReadIntOption(settings, kStickerHeightKey, SetStickerHeight);

		ReadStringOption(settings, "radio_controller", [&](auto v) {
			if (v.isEmpty()) {
				SetEnhancedValue("radio_controller", "http://localhost:2468");
			}
		});

		return true;
	}

	void Manager::addIdToBlocklist(int64 userId) {
		QFile file(cWorkingDir() + qsl("tdata/blocklist.json"));
		if (file.open(QIODevice::WriteOnly)) {
			auto toArray = [&] {
				QJsonArray array;
				for (auto id : cBlockList()) {
					array.append(id);
				}
				array.append(userId);
				return array;
			};
			auto doc = QJsonDocument(toArray());
			file.write(doc.toJson(QJsonDocument::Compact));
			file.close();
			readBlocklist();
		}
	}

	void Manager::removeIdFromBlocklist(int64 userId) {
		QFile file(cWorkingDir() + qsl("tdata/blocklist.json"));
		if (file.open(QIODevice::WriteOnly)) {
			auto toArray = [&] {
				QJsonArray array;
				for (auto id : cBlockList()) {
					if (id != userId) {
						array.append(id);
					}
				}
				return array;
			};
			auto doc = QJsonDocument(toArray());
			file.write(doc.toJson(QJsonDocument::Compact));
			file.close();
			readBlocklist();
		}
	}

	void Manager::readBlocklist() {
		QFile block(cWorkingDir() + qsl("tdata/blocklist.json"));
		if (block.open(QIODevice::ReadOnly)) {
			auto doc = QJsonDocument::fromJson(block.readAll());
			block.close();
			auto toList = [=] {
				QList<int64> blockList;
				for (const auto id : doc.array()) {
					blockList.append(int64(id.toDouble()));
				}
				return blockList;
			};
			cSetBlockList(toList());
		}
	}

	void Manager::writeDefaultFile() {
		auto file = QFile(DefaultFilePath());
		if (!file.open(QIODevice::WriteOnly)) {
			return;
		}
		const char *defaultHeader = R"HEADER(
// This is a list of default options for 64Gram Desktop
// Please don't modify it, its content is not used in any way
// You can place your own options in the 'enhanced-settings-custom.json' file
)HEADER";
		file.write(defaultHeader);

		auto settings = QJsonObject();
		settings.insert(qsl("show_messages_id"), false);
		settings.insert(qsl("extra_context_menu_options"), QJsonArray());
		settings.insert(qsl("show_emoji_button_as_text"), false);
		settings.insert(qsl("show_phone_number"), true);
		settings.insert(qsl("repeater_reply_to_orig_msg"), false);
		settings.insert(qsl("disable_cloud_draft_sync"), false);
		settings.insert(qsl("disable_sync_draft_to_cloud"), false);
		settings.insert(qsl("show_scheduled_button"), false);
		settings.insert(qsl("stereo_mode"), false);
		settings.insert(qsl("radio_controller"), "http://localhost:2468");
		settings.insert(qsl("auto_unmute"), false);
		settings.insert(qsl("bitrate"), 0);
		settings.insert(qsl("hide_all_chats"), false);
		settings.insert(qsl("replace_edit_button"), false);
		settings.insert(qsl("hd_video"), false);
		settings.insert(qsl("skip_to_next"), false);
		settings.insert(qsl("disable_link_warning"), false);
		settings.insert(qsl("blocked_user_spoiler_mode"), false);
		settings.insert(qsl("disable_premium_animation"), false);
		settings.insert(qsl("disable_global_search"), false);
		settings.insert(qsl("show_media_metadata"), false);
		settings.insert(qsl("community_chat_click"), false);
		settings.insert(qsl("show_group_sender_avatar"), false);
		settings.insert(qsl("show_group_sender_online_status"), false);
		settings.insert(qsl("show_seconds"), false);
		settings.insert(qsl("rich_message_preview_max_blocks"), 0);
		settings.insert(qsl("force_show_webpage_preview"), false);
		settings.insert(qsl("disable_auto_fetch_webpage_preview"), false);
		settings.insert(qsl("remove_media_spoiler"), false);
		settings.insert(qsl("hide-delete-for-others-checkbox"), false);
		settings.insert(qsl("sticker_height"), 0);
		settings.insert(qsl("hide_counter"), false);
		settings.insert(qsl("use_gt_api"), false);
		settings.insert(qsl("translate_to_tc"), false);
		settings.insert(qsl("hide_stories"), false);
		settings.insert(qsl("force_mobile"), false);
		settings.insert(qsl("show_similar_on_joined"), false);
		settings.insert(qsl("more_right_action_comments"), false);
		settings.insert(qsl("send_comment_after_forwarding"), false);
		settings.insert(qsl("mpris_call_hangup"), false);
		settings.insert(qsl("screenshot_mode"), false);
		settings.insert(qsl("allow_screenshot"), false);
        settings.insert(qsl("hide_star_ratings"), false);
		settings.insert(qsl("show_peer_id"), false);

		auto document = QJsonDocument();
		document.setObject(settings);
		file.write(document.toJson(QJsonDocument::Indented));

		loadSettings(settings);
	}

	void Manager::writeCurrentSettings() {
		auto file = QFile(CustomFilePath());
		if (!file.open(QIODevice::WriteOnly)) {
			return;
		}
		if (_jsonWriteTimer.isActive()) {
			writing();
		}
		const char *customHeader = R"HEADER(
// This file was automatically generated from current settings
// It's better to edit it with app closed, so there will be no rewrites
// You should restart app to see changes
)HEADER";
		file.write(customHeader);

		auto settings = QJsonObject();
		settings.insert(qsl("show_messages_id"), GetEnhancedBool("show_messages_id"));
		{
			QJsonArray arr;
			for (const auto &v : GetEnhancedIntList("extra_context_menu_options")) {
				arr.append(v);
			}
			settings.insert(qsl("extra_context_menu_options"), arr);
		}
		settings.insert(qsl("show_emoji_button_as_text"), GetEnhancedBool("show_emoji_button_as_text"));
		settings.insert(qsl("show_phone_number"), GetEnhancedBool("show_phone_number"));
		settings.insert(qsl("repeater_reply_to_orig_msg"), GetEnhancedBool("repeater_reply_to_orig_msg"));
		settings.insert(qsl("disable_cloud_draft_sync"), GetEnhancedBool("disable_cloud_draft_sync"));
		settings.insert(qsl("disable_sync_draft_to_cloud"), GetEnhancedBool("disable_sync_draft_to_cloud"));
		settings.insert(qsl("show_scheduled_button"), GetEnhancedBool("show_scheduled_button"));
		settings.insert(qsl("stereo_mode"), GetEnhancedBool("stereo_mode"));
		settings.insert(qsl("radio_controller"), GetEnhancedString("radio_controller"));
		settings.insert(qsl("auto_unmute"), GetEnhancedBool("auto_unmute"));
		settings.insert(qsl("bitrate"), GetEnhancedInt("bitrate"));
		settings.insert(qsl("hide_all_chats"), GetEnhancedBool("hide_all_chats"));
		settings.insert(qsl("replace_edit_button"), GetEnhancedBool("replace_edit_button"));
		settings.insert(qsl("hd_video"), GetEnhancedBool("hd_video"));
		settings.insert(qsl("skip_to_next"), GetEnhancedBool("skip_to_next"));
		settings.insert(qsl("disable_link_warning"), GetEnhancedBool("disable_link_warning"));
		settings.insert(qsl("blocked_user_spoiler_mode"), GetEnhancedBool("blocked_user_spoiler_mode"));
		settings.insert(qsl("disable_premium_animation"), GetEnhancedBool("disable_premium_animation"));
		settings.insert(qsl("disable_global_search"), GetEnhancedBool("disable_global_search"));
		settings.insert(qsl("show_media_metadata"), GetEnhancedBool("show_media_metadata"));
		settings.insert(qsl("community_chat_click"), GetEnhancedBool("community_chat_click"));
		settings.insert(qsl("show_group_sender_avatar"), GetEnhancedBool("show_group_sender_avatar"));
		settings.insert(qsl("show_group_sender_online_status"), ShowGroupSenderOnlineStatus());
		settings.insert(qsl("show_seconds"), GetEnhancedBool("show_seconds"));
		settings.insert(qsl("rich_message_preview_max_blocks"), RichMessagePreviewBlocksLimit());
		settings.insert(qsl("force_show_webpage_preview"), GetEnhancedBool("force_show_webpage_preview"));
		settings.insert(qsl("disable_auto_fetch_webpage_preview"), GetEnhancedBool("disable_auto_fetch_webpage_preview"));
		settings.insert(qsl("remove_media_spoiler"), GetEnhancedBool("remove_media_spoiler"));
		settings.insert(qsl("hide-delete-for-others-checkbox"), GetEnhancedBool("hide-delete-for-others-checkbox"));
		settings.insert(qsl("sticker_height"), StickerHeight());
		settings.insert(qsl("hide_counter"), GetEnhancedBool("hide_counter"));
		settings.insert(qsl("use_gt_api"), GetEnhancedBool("use_gt_api"));
		settings.insert(qsl("translate_to_tc"), GetEnhancedBool("translate_to_tc"));
		settings.insert(qsl("hide_stories"), GetEnhancedBool("hide_stories"));
		settings.insert(qsl("force_mobile"), GetEnhancedBool("force_mobile"));
		settings.insert(qsl("show_similar_on_joined"), GetEnhancedBool("show_similar_on_joined"));
		settings.insert(qsl("more_right_action_comments"), GetEnhancedBool("more_right_action_comments"));
		settings.insert(
			qsl("send_comment_after_forwarding"),
			GetEnhancedBool("send_comment_after_forwarding"));
		settings.insert(qsl("mpris_call_hangup"), GetEnhancedBool("mpris_call_hangup"));
		settings.insert(qsl("screenshot_mode"), GetEnhancedBool("screenshot_mode"));
		settings.insert(qsl("allow_screenshot"), AllowScreenshots());
        settings.insert(qsl("hide_star_ratings"), GetEnhancedBool("hide_star_ratings"));
		settings.insert(qsl("show_peer_id"), GetEnhancedBool("show_peer_id"));

		auto document = QJsonDocument();
		document.setObject(settings);
		file.write(document.toJson(QJsonDocument::Indented));
	}

	void Manager::writeTimeout() {
		writeCurrentSettings();
	}

	void Manager::writing() {
		_jsonWriteTimer.stop();
	}

	void Start() {
		if (Data) return;

		Data = std::make_unique<Manager>();
		Data->fill();
	}

	void Write() {
		if (!Data) return;

		Data->write();
	}

	void Reset() {
		if (!Data) return;

		Data->reset();
	}

	void Finish() {
		if (!Data) return;

		Data->write(true);
	}

} // namespace EnhancedSettings
