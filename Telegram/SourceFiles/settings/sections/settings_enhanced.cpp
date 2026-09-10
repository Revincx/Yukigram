/*
This file is part of 64Gram Desktop,
the unofficial app based on Telegram Desktop.
For license and copyright information please follow this link:
https://github.com/TDesktop-x64/tdesktop/blob/dev/LEGAL
*/
#include <base/timer_rpl.h>
#include <ui/toast/toast.h>
#include <mainwindow.h>
#include <QJsonArray>
#include <QJsonDocument>
#include <QtGui/QGuiApplication>
#include "settings/sections/settings_enhanced.h"

#include "settings/settings_common.h"
#include "settings/settings_builder.h"
#include "settings/sections/settings_chat.h"
#include "settings/sections/settings_main.h"
#include <ui/vertical_list.h>
#include "ui/wrap/vertical_layout.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/continuous_sliders.h"
#include "ui/widgets/menu/menu_add_action_callback.h"
#include "ui/widgets/popup_menu.h"
#include "ui/text/text_entity.h"
#include "ui/text/text_utilities.h" // Ui::Text::ToUpper
#include "boxes/connection_box.h"
#include "boxes/enhanced_options_box.h"
#include "boxes/link_preview_rules_box.h"
#include "ui/layers/generic_box.h"
#include "boxes/about_box.h"
#include "ui/boxes/confirm_box.h"
#include "platform/platform_specific.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"
#include "lang/lang_keys.h"
#include "lang/lang_instance.h"
#include "core/application.h"
#include "core/chat_enhanced_settings.h"
#include "core/enhanced_settings.h"
#include "core/update_checker.h"
#include "storage/localstorage.h"
#include "data/data_session.h"
#include "data/data_histories.h"
#include "history/history.h"
#include "iv/iv_instance.h"
#include "main/main_session.h"
#include "layout/layout_item_base.h"
#include "facades.h"
#include "styles/style_chat_helpers.h"
#include "styles/style_settings.h"
#include "styles/style_menu_icons.h"
#include "apiwrap.h"
#include "api/api_blocked_peers.h"

namespace Settings {

namespace {

const auto kEnhancedSettingsClipboardPrefix
	= u"yurigram-settings:"_q;

struct DecodeEnhancedSettingsResult {
	bool ok = false;
	QString json;
};

[[nodiscard]] QString EncodeEnhancedSettingsToText(const QString &json) {
	const auto flags = QByteArray::Base64UrlEncoding
		| QByteArray::OmitTrailingEquals;
	return kEnhancedSettingsClipboardPrefix
		+ qs(qCompress(json.toUtf8(), 9).toBase64(flags));
}

[[nodiscard]] DecodeEnhancedSettingsResult DecodeEnhancedSettingsFromText(
		const QString &text) {
	auto result = DecodeEnhancedSettingsResult();
	if (!text.startsWith(kEnhancedSettingsClipboardPrefix)) {
		return result;
	}
	auto encoded = QStringView(text).mid(
		kEnhancedSettingsClipboardPrefix.size()).toLatin1();
	const auto compressed = QByteArray::fromBase64Encoding(
		std::move(encoded),
		QByteArray::Base64UrlEncoding
			| QByteArray::AbortOnBase64DecodingErrors);
	if (!compressed || (*compressed).isEmpty()) {
		return result;
	}
	const auto decoded = qUncompress(*compressed);
	if (decoded.isEmpty()) {
		return result;
	}

	auto error = QJsonParseError();
	const auto parsed = QJsonDocument::fromJson(decoded, &error);
	if (error.error != QJsonParseError::NoError || !parsed.isObject()) {
		return result;
	}
	result.ok = true;
	result.json = QString::fromUtf8(decoded);
	return result;
}

[[nodiscard]] QString StickerHeightLabel(int height) {
	return tr::lng_settings_sticker_height_pixels(
		tr::now,
		lt_height,
		QString::number(height));
}

[[nodiscard]] QString ConfigForTLViewer(const MTPConfig &config) {
	auto buffer = mtpBuffer();
	config.write(buffer);
	const auto bytes = QByteArray(
		reinterpret_cast<const char*>(buffer.constData()),
		buffer.size() * sizeof(mtpPrime));
	return QString::fromLatin1(
		bytes.toBase64(QByteArray::Base64UrlEncoding));
}

[[nodiscard]] not_null<Button*> AddEnhancedOptionRow(
		not_null<Ui::VerticalLayout*> container,
		rpl::producer<QString> titleText,
		rpl::producer<QString> aboutText,
		const style::SettingsButton &buttonStyle,
		const style::FlatLabel &titleStyle = st::settingsExperimentalTitle) {
	const auto &titlePadding = st::settingsExperimentalTitlePadding;
	const auto &aboutPadding = st::settingsExperimentalAboutPadding;
	const auto button = Ui::CreateChild<Button>(
		container.get(),
		rpl::single(QString()),
		buttonStyle);
	const auto title = container->add(
		object_ptr<Ui::FlatLabel>(
			container,
			std::move(titleText),
			titleStyle),
		titlePadding);
	const auto about = container->add(
		object_ptr<Ui::FlatLabel>(
			container,
			std::move(aboutText),
			st::settingsExperimentalAbout),
		aboutPadding);
	title->setAttribute(Qt::WA_TransparentForMouseEvents);
	about->setAttribute(Qt::WA_TransparentForMouseEvents);
	rpl::combine(
		container->widthValue(),
		title->heightValue(),
		about->heightValue()
	) | rpl::on_next([=](int width, int titleHeight, int aboutHeight) {
		button->resize(width, titlePadding.top()
			+ titleHeight
			+ titlePadding.bottom()
			+ aboutPadding.top()
			+ aboutHeight
			+ aboutPadding.bottom());
	}, button->lifetime());
	title->topValue(
	) | rpl::on_next([=](int top) {
		button->moveToLeft(0, top - titlePadding.top());
	}, button->lifetime());
	button->show();
	return button;
}

[[nodiscard]] not_null<Ui::VerticalLayout*> AddEnhancedGroup(
		not_null<Ui::VerticalLayout*> page,
		rpl::producer<QString> title) {
	AddDivider(page);
	AddSkip(page);
	AddSubsectionTitle(page, std::move(title));
	const auto wrap = page->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			page,
			object_ptr<Ui::VerticalLayout>(page)));
	return wrap->entity();
}

[[maybe_unused]] const auto kEnhancedMeta = Builder::BuildHelper({
	.id = Enhanced::Id(),
	.parentId = MainId(),
	.title = &tr::lng_settings_enhanced,
	.icon = &st::menuIconManage,
}, [](Builder::SectionBuilder &builder) {
	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/show-message-id"_q,
			.title = tr::lng_settings_show_message_id(tr::now),
			.keywords = { u"message"_q, u"id"_q, u"info"_q },
			.deeplink = u"tg://settings/enhanced/show-message-id"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/rich-message-blocks-limit"_q,
			.title = tr::lng_settings_rich_message_preview_blocks(tr::now),
			.keywords = {
				u"rich message"_q,
				u"blocks"_q,
				u"preview"_q,
				u"show more"_q,
			},
			.deeplink
				= u"tg://settings/enhanced/rich-message-blocks-limit"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/sticker-height"_q,
			.title = tr::lng_settings_sticker_height(tr::now),
			.keywords = { u"sticker"_q, u"height"_q, u"size"_q },
			.deeplink = u"tg://settings/enhanced/sticker-height"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/show-similar-on-joined"_q,
			.title = tr::lng_settings_show_similar_on_joined(tr::now),
			.keywords = { u"similar"_q, u"recommendations"_q, u"joined"_q },
			.deeplink = u"tg://settings/enhanced/show-similar-on-joined"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/more-right-action-comments"_q,
			.title = tr::lng_settings_more_right_action_comments(tr::now),
			.keywords = { u"context"_q, u"menu"_q, u"comments"_q },
			.deeplink = u"tg://settings/enhanced/more-right-action-comments"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/send-comment-after-forwarding"_q,
			.title = tr::lng_settings_send_comment_after_forwarding(tr::now),
			.keywords = { u"forward"_q, u"comment"_q, u"order"_q },
			.deeplink
				= u"tg://settings/enhanced/send-comment-after-forwarding"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/extra-context-menu-options"_q,
			.title = tr::lng_settings_extra_context_menu_options(tr::now),
			.keywords = {
				u"forward"_q,
				u"repeater"_q,
				u"json"_q,
				u"context"_q,
				u"menu"_q,
			},
			.deeplink = u"tg://settings/enhanced/extra-context-menu-options"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/repeater-reply-to-original"_q,
			.title = tr::lng_settings_repeater_reply_to_orig_msg(tr::now),
			.keywords = { u"repeater"_q, u"reply"_q, u"original"_q },
			.deeplink = u"tg://settings/enhanced/repeater-reply-to-original"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/disable-cloud-draft-sync"_q,
			.title = tr::lng_settings_disable_cloud_draft_sync(tr::now),
			.keywords = { u"draft"_q, u"cloud"_q, u"sync"_q },
			.deeplink = u"tg://settings/enhanced/disable-cloud-draft-sync"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/disable-sync-draft-to-cloud"_q,
			.title = tr::lng_settings_disable_sync_draft_to_cloud(tr::now),
			.keywords = { u"draft"_q, u"cloud"_q, u"sync"_q, u"local"_q },
			.deeplink
				= u"tg://settings/enhanced/disable-sync-draft-to-cloud"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/force-show-webpage-preview"_q,
			.title = tr::lng_settings_force_show_webpage_preview(tr::now),
			.keywords = {
				u"force"_q,
				u"link"_q,
				u"preview"_q,
				u"show"_q,
				u"url"_q,
				u"web page"_q,
				u"webpage"_q,
			},
			.deeplink
				= u"tg://settings/enhanced/force-show-webpage-preview"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/disable-auto-fetch-webpage-preview"_q,
			.title = tr::lng_settings_disable_auto_fetch_webpage_preview(
				tr::now),
			.keywords = {
				u"automatic"_q,
				u"disable"_q,
				u"fetch"_q,
				u"link"_q,
				u"preview"_q,
				u"url"_q,
				u"web page"_q,
				u"webpage"_q,
			},
			.deeplink
				= u"tg://settings/enhanced/disable-auto-fetch-webpage-preview"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/remove-media-spoiler"_q,
			.title = tr::lng_settings_remove_media_spoiler(tr::now),
			.keywords = {
				u"media"_q,
				u"spoiler"_q,
				u"remove"_q,
				u"show"_q,
			},
			.deeplink = u"tg://settings/enhanced/remove-media-spoiler"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/hide-delete-for-others-checkbox"_q,
			.title = tr::lng_settings_hide_delete_for_others_checkbox(tr::now),
			.keywords = {
				u"delete"_q,
				u"private"_q,
				u"chat"_q,
				u"other"_q,
			},
			.deeplink
				= u"tg://settings/enhanced/hide-delete-for-others-checkbox"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/link-preview-replacements"_q,
			.title = tr::lng_link_preview_rules_title(tr::now),
			.keywords = { u"link"_q, u"preview"_q, u"domain"_q, u"regex"_q },
			.deeplink = u"tg://settings/enhanced/link-preview-replacements"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/disable-link-warning"_q,
			.title = tr::lng_settings_disable_link_warning(tr::now),
			.keywords = { u"link"_q, u"warning"_q, u"confirm"_q },
			.deeplink = u"tg://settings/enhanced/disable-link-warning"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/disable-premium-animation"_q,
			.title = tr::lng_settings_disable_premium_animation(tr::now),
			.keywords = { u"premium"_q, u"animation"_q, u"effects"_q },
			.deeplink = u"tg://settings/enhanced/disable-premium-animation"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/disable-global-search"_q,
			.title = tr::lng_settings_disable_global_search(tr::now),
			.keywords = { u"global"_q, u"search"_q, u"server"_q },
			.deeplink = u"tg://settings/enhanced/disable-global-search"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/show-media-metadata"_q,
			.title = tr::lng_settings_show_media_metadata(tr::now),
			.keywords = { u"media"_q, u"metadata"_q, u"codec"_q },
			.deeplink = u"tg://settings/enhanced/show-media-metadata"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/show-group-sender-avatar"_q,
			.title = tr::lng_settings_show_group_sender_avatar(tr::now),
			.keywords = { u"group"_q, u"sender"_q, u"avatar"_q },
			.deeplink = u"tg://settings/enhanced/show-group-sender-avatar"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/show-group-sender-online-status"_q,
			.title = tr::lng_settings_show_group_sender_online_status(tr::now),
			.keywords = {
				u"group"_q,
				u"sender"_q,
				u"avatar"_q,
				u"online"_q,
				u"status"_q,
			},
			.deeplink
				= u"tg://settings/enhanced/show-group-sender-online-status"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/use-gt-api"_q,
			.title = tr::lng_settings_use_gt_api(tr::now),
			.keywords = { u"translate"_q, u"google"_q, u"api"_q, u"gt"_q },
			.deeplink = u"tg://settings/enhanced/use-gt-api"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/translate-to-tc"_q,
			.title = tr::lng_settings_translate_to_tc(tr::now),
			.keywords = { u"translate"_q, u"traditional"_q, u"chinese"_q, u"tc"_q },
			.deeplink = u"tg://settings/enhanced/translate-to-tc"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/show-seconds"_q,
			.title = tr::lng_settings_show_seconds(tr::now),
			.keywords = { u"seconds"_q, u"clock"_q, u"time"_q },
			.deeplink = u"tg://settings/enhanced/show-seconds"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/hide-blocked-messages"_q,
			.title = tr::lng_settings_hide_messages(tr::now),
			.keywords = { u"blocked"_q, u"messages"_q, u"spoiler"_q, u"hide"_q },
			.deeplink = u"tg://settings/enhanced/hide-blocked-messages"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/show-emoji-button-as-text"_q,
			.title = tr::lng_settings_show_emoji_button_as_text(tr::now),
			.keywords = { u"emoji"_q, u"button"_q, u"text"_q },
			.deeplink = u"tg://settings/enhanced/show-emoji-button-as-text"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/show-scheduled-button"_q,
			.title = tr::lng_settings_show_scheduled_button(tr::now),
			.keywords = { u"scheduled"_q, u"schedule"_q, u"button"_q },
			.deeplink = u"tg://settings/enhanced/show-scheduled-button"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/radio-controller"_q,
			.title = tr::lng_settings_radio_controller(tr::now),
			.keywords = { u"radio"_q, u"voice"_q, u"controller"_q },
			.deeplink = u"tg://settings/enhanced/radio-controller"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/auto-unmute"_q,
			.title = tr::lng_settings_auto_unmute(tr::now),
			.keywords = { u"auto"_q, u"unmute"_q, u"voice"_q },
			.deeplink = u"tg://settings/enhanced/auto-unmute"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/bitrate-controller"_q,
			.title = tr::lng_bitrate_controller(tr::now),
			.keywords = { u"bitrate"_q, u"audio"_q, u"quality"_q },
			.deeplink = u"tg://settings/enhanced/bitrate-controller"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/enable-hd-video"_q,
			.title = tr::lng_settings_enable_hd_video(tr::now),
			.keywords = { u"hd"_q, u"video"_q, u"quality"_q },
			.deeplink = u"tg://settings/enhanced/enable-hd-video"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/mpris-call-hangup"_q,
			.title = tr::lng_settings_mpris_call_hangup(tr::now),
			.keywords = { u"mpris"_q, u"media"_q, u"hangup"_q },
			.deeplink = u"tg://settings/enhanced/mpris-call-hangup"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/show-peer-id"_q,
			.title = tr::lng_settings_show_peer_id(tr::now),
			.keywords = { u"peer"_q, u"id"_q, u"user"_q, u"chat"_q },
			.deeplink = u"tg://settings/enhanced/show-peer-id"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/hide-all-chats"_q,
			.title = tr::lng_settings_hide_all_chats(tr::now),
			.keywords = { u"hide"_q, u"all"_q, u"chats"_q },
			.deeplink = u"tg://settings/enhanced/hide-all-chats"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/replace-edit-button"_q,
			.title = tr::lng_settings_replace_edit_button(tr::now),
			.keywords = { u"edit"_q, u"button"_q, u"replace"_q },
			.deeplink = u"tg://settings/enhanced/replace-edit-button"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/skip-message"_q,
			.title = tr::lng_settings_skip_message(tr::now),
			.keywords = { u"skip"_q, u"message"_q, u"next"_q },
			.deeplink = u"tg://settings/enhanced/skip-message"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/hide-counter"_q,
			.title = tr::lng_settings_hide_counter(tr::now),
			.keywords = { u"unread"_q, u"counter"_q, u"badge"_q },
			.deeplink = u"tg://settings/enhanced/hide-counter"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/hide-stories"_q,
			.title = tr::lng_settings_hide_stories(tr::now),
			.keywords = { u"stories"_q, u"hide"_q },
			.deeplink = u"tg://settings/enhanced/hide-stories"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/hide-star-ratings"_q,
			.title = tr::lng_settings_hide_star_ratings(tr::now),
			.keywords = { u"stars"_q, u"ratings"_q, u"hide"_q },
			.deeplink = u"tg://settings/enhanced/hide-star-ratings"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/force-mobile"_q,
			.title = tr::lng_settings_force_mobile(tr::now),
			.keywords = { u"mobile"_q, u"layout"_q, u"force"_q },
			.deeplink = u"tg://settings/enhanced/force-mobile"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/community-chat-click"_q,
			.title = tr::lng_settings_community_chat_click(tr::now),
			.keywords = {
				u"chat"_q,
				u"community"_q,
				u"avatar"_q,
				u"userpic"_q,
				u"profile"_q,
			},
			.deeplink
				= u"tg://settings/enhanced/community-chat-click"_q,
		};
	});

	builder.add(nullptr, [] {
		return Builder::SearchEntry{
			.id = u"enhanced/show-server-config"_q,
			.title = tr::lng_settings_show_server_config(tr::now),
			.keywords = { u"server"_q, u"config"_q, u"tlv"_q },
			.deeplink = u"tg://settings/enhanced/show-server-config"_q,
		};
	});
});

} // namespace

	Type EnhancedId() {
		return Enhanced::Id();
	}

	not_null<Button*> Enhanced::addOptionRow(
			not_null<Ui::VerticalLayout*> content,
			EnhancedSettings::OptionId id,
			std::optional<rpl::producer<QString>> about) {
		const auto &descriptor = EnhancedSettings::DescriptorFor(id);
		const auto title = descriptor.title;
		Expects(title != nullptr);
		const auto &buttonStyle = descriptor.restartRequired
			? st::settingsAttentionButton
			: st::settingsButtonNoIcon;
		const auto button = about
			? AddEnhancedOptionRow(
				content,
				(*title)(),
				std::move(*about),
				buttonStyle,
				descriptor.restartRequired
					? st::settingsEnhancedAttentionTitle
					: st::settingsExperimentalTitle)
			: AddButtonWithIcon(content, (*title)(), buttonStyle);
		registerHighlight(id, button);
		return button;
	}

	void Enhanced::addToggleOption(
			not_null<Ui::VerticalLayout*> content,
			EnhancedSettings::Key<bool> key,
			std::optional<rpl::producer<QString>> about) {
		const auto button = addOptionRow(content, key.id, std::move(about));
		button->toggleOn(
			EnhancedSettings::Watch(key)
		)->toggledChanges(
		) | rpl::filter([=](bool value) {
			return value != EnhancedSettings::Get(key);
		}) | rpl::on_next([=](bool value) {
			EnhancedSettings::ApplyOption(controller(), key, value);
		}, content->lifetime());
	}

	void Enhanced::addActionOption(
			not_null<Ui::VerticalLayout*> content,
			EnhancedSettings::OptionId id,
			Fn<void()> handler,
			std::optional<rpl::producer<QString>> about) {
		addOptionRow(content, id, std::move(about))->addClickHandler(
			std::move(handler));
	}

	not_null<Button*> Enhanced::addLabeledOption(
			not_null<Ui::VerticalLayout*> content,
			EnhancedSettings::OptionId id,
			rpl::producer<QString> label) {
		const auto &descriptor = EnhancedSettings::DescriptorFor(id);
		const auto title = descriptor.title;
		Expects(title != nullptr);
		const auto &buttonStyle = descriptor.restartRequired
			? st::settingsAttentionButton
			: st::settingsButtonNoIcon;
		const auto button = AddButtonWithLabel(
			content,
			(*title)(),
			std::move(label),
			buttonStyle);
		registerHighlight(id, button);
		return button;
	}

	void Enhanced::addLabeledActionOption(
			not_null<Ui::VerticalLayout*> content,
			EnhancedSettings::OptionId id,
			rpl::producer<QString> label,
			Fn<void()> handler) {
		addLabeledOption(
			content,
			id,
			std::move(label))->addClickHandler(std::move(handler));
	}

	void Enhanced::addIntegerSliderOption(
			not_null<Ui::VerticalLayout*> content,
			EnhancedSettings::Key<int> key,
			Fn<QString(int)> label,
			bool zeroAsMaximum) {
		const auto constraint = EnhancedSettings::IntegerConstraintFor(key);
		Expects(constraint.minimum < constraint.maximum);
		const auto displayValue = [=] {
			const auto stored = EnhancedSettings::Get(key);
			return (!stored && zeroAsMaximum) ? constraint.maximum : stored;
		};
		const auto valueLabel = content->lifetime(
		).make_state<rpl::event_stream<QString>>();
		addLabeledOption(
			content,
			key.id,
			valueLabel->events_starting_with(label(displayValue())));

		const auto slider = content->add(
			object_ptr<Ui::MediaSliderWheelless>(content, st::settingsScale),
			st::settingsBigScalePadding);
		slider->resize(slider->width(), st::settingsScale.seekSize.height());
		slider->setAccessibleName(EnhancedSettings::OptionTitle(key.id));

		const auto fromIndex = [=](int index) {
			return constraint.minimum + index;
		};
		const auto toIndex = [=](int value) {
			return value - constraint.minimum;
		};
		const auto sections = constraint.maximum - constraint.minimum;
		slider->setPseudoDiscrete(
			sections + 1,
			[](int index) { return index; },
			toIndex(displayValue()),
			[=](int index) { valueLabel->fire(label(fromIndex(index))); },
			[=](int index) {
				EnhancedSettings::ApplyOption(
					controller(),
					key,
					fromIndex(index));
			});
		EnhancedSettings::Changes(
			key
		) | rpl::on_next([=] {
			const auto value = displayValue();
			slider->setValue(toIndex(value) / float64(sections));
			valueLabel->fire(label(value));
		}, content->lifetime());
	}

	void Enhanced::setupMessages(not_null<Ui::VerticalLayout*> content) {
		addToggleOption(content, EnhancedSettings::Option::ShowMessagesId);
		addToggleOption(
			content,
			EnhancedSettings::Option::ForceShowWebPagePreview);
		addToggleOption(
			content,
			EnhancedSettings::Option::DisableAutoFetchWebPagePreview);
		addToggleOption(content, EnhancedSettings::Option::RemoveMediaSpoiler);
		addToggleOption(content, EnhancedSettings::Option::ShowMediaMetadata);
		addToggleOption(content, EnhancedSettings::Option::HideBlockedMessages);

		auto richMessagePreviewBlocksValue = rpl::combine(
			tr::lng_font_default(),
			EnhancedSettings::Watch(
				EnhancedSettings::Option::RichMessagePreviewBlocksLimit)
		) | rpl::map([](QString defaultLabel, int limit) {
			return limit ? QString::number(limit) : std::move(defaultLabel);
		});
		addLabeledActionOption(
			content,
			EnhancedSettings::Option::RichMessagePreviewBlocksLimit.id,
			std::move(richMessagePreviewBlocksValue),
			[=] { Ui::show(Box<RichMessagePreviewBlocksBox>()); });
	}

	void Enhanced::setupInterface(not_null<Ui::VerticalLayout*> content) {
		addToggleOption(
			content,
			EnhancedSettings::Option::DisablePremiumAnimation);
		addToggleOption(content, EnhancedSettings::Option::ShowGroupSenderAvatar);
		addToggleOption(
			content,
			EnhancedSettings::Option::ShowGroupSenderOnlineStatus);
		addToggleOption(content, EnhancedSettings::Option::ShowSeconds);

		addIntegerSliderOption(
			content,
			EnhancedSettings::Option::StickerHeight,
			StickerHeightLabel,
			true);

		addToggleOption(
			content,
			EnhancedSettings::Option::ShowEmojiButtonAsText,
			tr::lng_show_emoji_button_as_text_desc());
		addToggleOption(content, EnhancedSettings::Option::ShowScheduledButton);
		addToggleOption(content, EnhancedSettings::Option::ShowPeerId);
		addToggleOption(content, EnhancedSettings::Option::HideAllChats);
		addToggleOption(content, EnhancedSettings::Option::HideCounter);
		addToggleOption(content, EnhancedSettings::Option::HideStories);
		addToggleOption(content, EnhancedSettings::Option::HideStarRatings);
		addToggleOption(content, EnhancedSettings::Option::ForceMobile);
		addToggleOption(
			content,
			EnhancedSettings::Option::HideDeleteForOthersCheckbox);
	}

	void Enhanced::setupBehavior(not_null<Ui::VerticalLayout*> content) {
		addToggleOption(content, EnhancedSettings::Option::ShowSimilarOnJoined);
		addToggleOption(
			content,
			EnhancedSettings::Option::MoreRightActionComments);
		addToggleOption(
			content,
			EnhancedSettings::Option::SendCommentAfterForwarding);
		addToggleOption(
			content,
			EnhancedSettings::Option::DisableCloudDraftSync);
		addToggleOption(
			content,
			EnhancedSettings::Option::DisableSyncDraftToCloud);
		addToggleOption(content, EnhancedSettings::Option::DisableLinkWarning);
		addToggleOption(content, EnhancedSettings::Option::DisableGlobalSearch);
		addActionOption(
			content,
			EnhancedSettings::Option::ExtraContextMenuOptions.id,
			[=] { Ui::show(Box<ExtraContextMenuBox>()); });

		const auto repeaterSubWrap = content->add(
			object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
				content,
				object_ptr<Ui::VerticalLayout>(content)));
		const auto repeaterContent = repeaterSubWrap->entity();
		repeaterSubWrap->toggleOn(
			EnhancedSettings::Watch(
				EnhancedSettings::Option::ExtraContextMenuOptions
			) | rpl::map([](const QList<int> &options) {
				return options.contains(int(
					EnhancedSettings::ExtraContextMenuOption::Repeater));
			}));

		addToggleOption(
			repeaterContent,
			EnhancedSettings::Option::RepeaterReplyToOriginal);
		addToggleOption(content, EnhancedSettings::Option::ReplaceEditButton);
		addToggleOption(
			content,
			EnhancedSettings::Option::SkipToNext,
			tr::lng_settings_skip_message_desc());
		addToggleOption(content, EnhancedSettings::Option::CommunityChatClick);
		addActionOption(
			content,
			EnhancedSettings::Option::LinkPreviewRules.id,
			[=] { Ui::show(Box(LinkPreviewRulesBox)); });
	}

	void Enhanced::setupTranslation(not_null<Ui::VerticalLayout*> content) {
		addToggleOption(content, EnhancedSettings::Option::UseGtApi);

		const auto langPackBaseId = Lang::GetInstance().baseId();
		if (langPackBaseId == u"zh-hant-raw"_q
			|| langPackBaseId == u"zh-hans-raw"_q) {
			addToggleOption(content, EnhancedSettings::Option::TranslateToTc);
		}
	}

	void Enhanced::setupVoiceChat(not_null<Ui::VerticalLayout*> page) {
		const auto voiceChatContent = AddEnhancedGroup(
			page,
			tr::lng_settings_voice_chat());

		addActionOption(
			voiceChatContent,
			EnhancedSettings::Option::RadioController.id,
			[=] { Ui::show(Box<RadioController>()); },
			tr::lng_radio_controller_desc());
		addToggleOption(
			voiceChatContent,
			EnhancedSettings::Option::AutoUnmute,
			tr::lng_auto_unmute_desc());
		addToggleOption(voiceChatContent, EnhancedSettings::Option::HdVideo);

		auto bitrateValue = EnhancedSettings::Watch(
			EnhancedSettings::Option::Bitrate
		) | rpl::map([](int bitrate) {
			return BitrateController::BitrateLabel(bitrate);
		});

		addLabeledActionOption(
			page,
			EnhancedSettings::Option::Bitrate.id,
			std::move(bitrateValue),
			[=] { Ui::show(Box<BitrateController>()); });
		addToggleOption(page, EnhancedSettings::Option::MprisCallHangup);

		AddSkip(page);
	}

	void Enhanced::setupOther(not_null<Ui::VerticalLayout*> content) {
		const auto showServerConfig = AddButtonWithIcon(
			content,
			tr::lng_settings_show_server_config(),
			st::settingsButtonNoIcon);
		registerHighlight(
			u"enhanced/show-server-config"_q,
			showServerConfig);
		showServerConfig->addClickHandler([=] {
			controller()->session().api().request(
				MTPhelp_GetConfig()
			).done(crl::guard(this, [=](const MTPConfig &config) {
				Core::App().iv().showTLViewer(
					MTP::details::kCurrentLayer,
					ConfigForTLViewer(config));
			})).fail(crl::guard(this, [=](const MTP::Error &error) {
				if (!MTP::IgnoreError(error)) {
					controller()->showToast(error.type());
				}
			})).send();
		});
	}

	rpl::producer<QString> Enhanced::title() {
		return tr::lng_settings_enhanced();
	}

	void Enhanced::fillTopBarMenu(
			const Ui::Menu::MenuCallback &addAction) {
		const auto window = &controller()->window();
		addAction(
			tr::lng_export_start(tr::now),
			[=] {
				TextUtilities::SetClipboardText({
					EncodeEnhancedSettingsToText(
						EnhancedSettings::Serialize()),
				});
				window->showToast({
					.text = {
						tr::lng_settings_enhanced_export_done(tr::now),
					},
					.iconLottie = u"toast/copy"_q,
					.iconLottieSize = st::toastLottieIconSize,
				});
			},
			&st::menuIconCopy);
		if (!DecodeEnhancedSettingsFromText(
				QGuiApplication::clipboard()->text()).ok) {
			return;
		}
		addAction(
			tr::lng_settings_enhanced_import(tr::now),
			[=] {
				const auto decoded = DecodeEnhancedSettingsFromText(
					QGuiApplication::clipboard()->text());
				if (!decoded.ok) {
					window->showToast(
						tr::lng_settings_enhanced_import_invalid(tr::now));
					return;
				}
				if (!EnhancedSettings::Deserialize(decoded.json)) {
					window->showToast(
						tr::lng_settings_enhanced_import_unsupported(
							tr::now));
					return;
				}
				window->showToast({
					.text = { tr::lng_settings_enhanced_import_done(tr::now) },
					.iconLottie = u"toast/save_to_gallery"_q,
					.iconLottieSize = st::toastLottieIconSize,
				});
			},
			&st::menuIconImportTheme);
	}

	Enhanced::Enhanced(
			QWidget *parent,
			not_null<Window::SessionController *> controller)
			: Section(parent, controller) {
		setupContent();
	}

	void Enhanced::setupContent() {
		const auto page = Ui::CreateChild<Ui::VerticalLayout>(this);

		setupMessages(AddEnhancedGroup(page, tr::lng_settings_messages()));
		setupInterface(AddEnhancedGroup(page, tr::lng_settings_interface()));
		setupBehavior(AddEnhancedGroup(page, tr::lng_settings_behavior()));
		setupTranslation(AddEnhancedGroup(page, tr::lng_settings_translation()));
		setupVoiceChat(page);
		setupOther(AddEnhancedGroup(page, tr::lng_settings_other()));

		Ui::ResizeFitChild(this, page);
	}

	void Enhanced::registerHighlight(
			EnhancedSettings::OptionId id,
			not_null<Ui::RpWidget*> widget) {
		registerHighlight(EnhancedSettings::ControlId(id), id, widget);
	}

	void Enhanced::registerHighlight(
			QString id,
			not_null<Ui::RpWidget*> widget) {
		registerHighlight(std::move(id), std::nullopt, widget);
	}

	void Enhanced::registerHighlight(
			QString id,
			std::optional<EnhancedSettings::OptionId> option,
			not_null<Ui::RpWidget*> widget) {
		_highlightControls.emplace_back(id, widget.get());

		const auto link = u"tg://settings/"_q + id;
		const auto menu = widget->lifetime(
		).make_state<base::unique_qptr<Ui::PopupMenu>>();
		widget->events(
		) | rpl::filter([](not_null<QEvent*> e) {
			return e->type() == QEvent::ContextMenu;
		}) | rpl::on_next([=](not_null<QEvent*> e) {
			*menu = base::make_unique_q<Ui::PopupMenu>(
				widget,
				st::popupMenuWithIcons);
			const auto copy = [=](QString value) {
				TextUtilities::SetClipboardText({ std::move(value) });
				controller()->showToast({
					.text = { tr::lng_username_copied(tr::now) },
					.iconLottie = u"toast/voip_invite"_q,
					.iconLottieSize = st::toastLottieIconSize,
				});
			};
			(*menu)->addAction(
				tr::lng_auction_menu_copy_link(tr::now),
				[=] { copy(link); },
				&st::menuIconCopy);
			if (option) {
				(*menu)->addAction(
					tr::lng_settings_share_current_setting(tr::now),
					[=] {
						copy(EnhancedSettings::DeepLinkWithCurrentValue(*option));
					},
					&st::menuIconCopy);
			}
			(*menu)->popup(QCursor::pos());
			e->accept();
		}, widget->lifetime());
	}

	void Enhanced::showFinished() {
		for (const auto &[id, widget] : _highlightControls) {
			if (widget) {
				controller()->checkHighlightControl(id, widget);
			}
		}
		Section<Enhanced>::showFinished();
	}
} // namespace Settings
