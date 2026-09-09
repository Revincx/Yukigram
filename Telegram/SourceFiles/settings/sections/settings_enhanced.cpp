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

[[nodiscard]] EnhancedSettings::IntegerConstraint StickerHeightConstraint() {
	return EnhancedSettings::IntegerConstraintFor(
		EnhancedSettings::Option::StickerHeight);
}

[[nodiscard]] int StickerHeightForIndex(int index) {
	return StickerHeightConstraint().minimum + index;
}

[[nodiscard]] int StickerHeightIndexForHeight(int height) {
	return height - StickerHeightConstraint().minimum;
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

	void Enhanced::setupMessages(not_null<Ui::VerticalLayout*> content) {
		const auto showMessageId = AddButtonWithIcon(
				content,
				tr::lng_settings_show_message_id(),
				st::settingsAttentionButton
		);
		registerHighlight(
			EnhancedSettings::Option::ShowMessagesId,
			showMessageId);
		showMessageId->toggleOn(
				rpl::single(EnhancedSettings::Get(EnhancedSettings::Option::ShowMessagesId))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != EnhancedSettings::Get(EnhancedSettings::Option::ShowMessagesId));
		}) | rpl::on_next([=](bool toggled) {
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::ShowMessagesId, toggled);
		}, content->lifetime());

		const auto forceShowWebPagePreview = AddButtonWithIcon(
				content,
				tr::lng_settings_force_show_webpage_preview(),
				st::settingsButtonNoIcon);
		registerHighlight(
			EnhancedSettings::Option::ForceShowWebPagePreview,
			forceShowWebPagePreview);
		forceShowWebPagePreview->toggleOn(
				rpl::single(EnhancedSettings::Get(EnhancedSettings::Option::ForceShowWebPagePreview))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled
				!= EnhancedSettings::Get(EnhancedSettings::Option::ForceShowWebPagePreview));
		}) | rpl::on_next([=](bool toggled) {
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::ForceShowWebPagePreview, toggled);
		}, content->lifetime());

		const auto disableAutoFetchWebPagePreview = AddButtonWithIcon(
				content,
				tr::lng_settings_disable_auto_fetch_webpage_preview(),
				st::settingsButtonNoIcon);
		registerHighlight(
			EnhancedSettings::Option::DisableAutoFetchWebPagePreview,
			disableAutoFetchWebPagePreview);
		disableAutoFetchWebPagePreview->toggleOn(
				rpl::single(EnhancedSettings::Get(EnhancedSettings::Option::DisableAutoFetchWebPagePreview))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != EnhancedSettings::Get(EnhancedSettings::Option::DisableAutoFetchWebPagePreview));
		}) | rpl::on_next([=](bool toggled) {
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::DisableAutoFetchWebPagePreview,
				toggled);
		}, content->lifetime());

		const auto removeMediaSpoiler = AddButtonWithIcon(
				content,
				tr::lng_settings_remove_media_spoiler(),
				st::settingsButtonNoIcon);
		registerHighlight(
			EnhancedSettings::Option::RemoveMediaSpoiler,
			removeMediaSpoiler);
		removeMediaSpoiler->toggleOn(
				rpl::single(EnhancedSettings::Get(EnhancedSettings::Option::RemoveMediaSpoiler))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != EnhancedSettings::Get(EnhancedSettings::Option::RemoveMediaSpoiler));
		}) | rpl::on_next([=](bool toggled) {
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::RemoveMediaSpoiler, toggled);
		}, content->lifetime());

		const auto showMediaMetadata = AddButtonWithIcon(
			content,
			tr::lng_settings_show_media_metadata(),
			st::settingsButtonNoIcon);
		registerHighlight(
			EnhancedSettings::Option::ShowMediaMetadata,
			showMediaMetadata);
		showMediaMetadata->toggleOn(
			rpl::single(EnhancedSettings::Get(EnhancedSettings::Option::ShowMediaMetadata))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return toggled != EnhancedSettings::Get(EnhancedSettings::Option::ShowMediaMetadata);
		}) | rpl::on_next([=](bool toggled) {
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::ShowMediaMetadata, toggled);
		}, content->lifetime());

		const auto hideBlockedMessages = AddButtonWithIcon(
			content,
			tr::lng_settings_hide_messages(),
			st::settingsButtonNoIcon
		);
		registerHighlight(
			EnhancedSettings::Option::HideBlockedMessages,
			hideBlockedMessages);
		hideBlockedMessages->toggleOn(
				rpl::single(EnhancedSettings::Get(EnhancedSettings::Option::HideBlockedMessages))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != EnhancedSettings::Get(EnhancedSettings::Option::HideBlockedMessages));
		}) | rpl::on_next([=](bool toggled) {
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::HideBlockedMessages, toggled);
		}, content->lifetime());

		auto richMessagePreviewBlocksValue = rpl::combine(
			tr::lng_font_default(),
			_RichMessagePreviewBlocksChanged.events_starting_with({})
		) | rpl::map([](QString defaultLabel, auto) {
			const auto limit
				= EnhancedSettings::Get(EnhancedSettings::Option::RichMessagePreviewBlocksLimit);
			return limit ? QString::number(limit) : std::move(defaultLabel);
		});
		const auto richMessagePreviewBlocks = AddButtonWithLabel(
			content,
			tr::lng_settings_rich_message_preview_blocks(),
			std::move(richMessagePreviewBlocksValue),
			st::settingsButtonNoIcon);
		registerHighlight(
			EnhancedSettings::Option::RichMessagePreviewBlocksLimit,
			richMessagePreviewBlocks);
		richMessagePreviewBlocks->events(
		) | rpl::on_next([=](not_null<QEvent*> e) {
			if (e->type() == QEvent::UpdateLater) {
				_RichMessagePreviewBlocksChanged.fire({});
			}
		}, content->lifetime());
		richMessagePreviewBlocks->addClickHandler([=] {
			Ui::show(Box<RichMessagePreviewBlocksBox>());
		});
	}

	void Enhanced::setupInterface(not_null<Ui::VerticalLayout*> content) {
		const auto disablePremiumAnimation = AddButtonWithIcon(
				content,
				tr::lng_settings_disable_premium_animation(),
				st::settingsButtonNoIcon
		);
		registerHighlight(
			EnhancedSettings::Option::DisablePremiumAnimation,
			disablePremiumAnimation);
		disablePremiumAnimation->toggleOn(
				rpl::single(EnhancedSettings::Get(EnhancedSettings::Option::DisablePremiumAnimation))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != EnhancedSettings::Get(EnhancedSettings::Option::DisablePremiumAnimation));
		}) | rpl::on_next([=](bool toggled) {
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::DisablePremiumAnimation, toggled);
		}, content->lifetime());

		const auto showGroupSenderAvatar = AddButtonWithIcon(
				content,
				tr::lng_settings_show_group_sender_avatar(),
				st::settingsButtonNoIcon
		);
		registerHighlight(
			EnhancedSettings::Option::ShowGroupSenderAvatar,
			showGroupSenderAvatar);
		showGroupSenderAvatar->toggleOn(
				rpl::single(EnhancedSettings::Get(EnhancedSettings::Option::ShowGroupSenderAvatar))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != EnhancedSettings::Get(EnhancedSettings::Option::ShowGroupSenderAvatar));
		}) | rpl::on_next([=](bool toggled) {
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::ShowGroupSenderAvatar, toggled);
		}, content->lifetime());

		const auto showGroupSenderOnlineStatus = AddButtonWithIcon(
				content,
				tr::lng_settings_show_group_sender_online_status(),
				st::settingsButtonNoIcon
		);
		registerHighlight(
			EnhancedSettings::Option::ShowGroupSenderOnlineStatus,
			showGroupSenderOnlineStatus);
		showGroupSenderOnlineStatus->toggleOn(
				EnhancedSettings::Watch(EnhancedSettings::Option::ShowGroupSenderOnlineStatus)
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled
				!= EnhancedSettings::Get(EnhancedSettings::Option::ShowGroupSenderOnlineStatus));
		}) | rpl::on_next([=](bool toggled) {
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::ShowGroupSenderOnlineStatus, toggled);
		}, content->lifetime());

		const auto showSeconds = AddButtonWithIcon(
			content,
			tr::lng_settings_show_seconds(),
			st::settingsAttentionButton
		);
		registerHighlight(
			EnhancedSettings::Option::ShowSeconds,
			showSeconds);
		showSeconds->toggleOn(
			rpl::single(EnhancedSettings::Get(EnhancedSettings::Option::ShowSeconds))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != EnhancedSettings::Get(EnhancedSettings::Option::ShowSeconds));
		}) | rpl::on_next([=](bool toggled) {
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::ShowSeconds, toggled);
		}, content->lifetime());

		const auto storedStickerHeight = EnhancedSettings::Get(EnhancedSettings::Option::StickerHeight);
		const auto stickerConstraint = StickerHeightConstraint();
		const auto currentStickerHeight = storedStickerHeight
			? storedStickerHeight
			: stickerConstraint.maximum;
		const auto stickerHeightLabel = content->lifetime(
		).make_state<rpl::event_stream<QString>>();
		const auto stickerHeight = AddButtonWithLabel(
			content,
			tr::lng_settings_sticker_height(),
			stickerHeightLabel->events_starting_with(
				StickerHeightLabel(currentStickerHeight)),
			st::settingsButtonNoIcon);
		registerHighlight(
			EnhancedSettings::Option::StickerHeight, stickerHeight);

		const auto slider = content->add(
			object_ptr<Ui::MediaSliderWheelless>(content, st::settingsScale),
			st::settingsBigScalePadding);
		slider->resize(slider->width(), st::settingsScale.seekSize.height());
		slider->setAccessibleName(tr::lng_settings_sticker_height(tr::now));

		const auto saveStickerHeight = [=](int height) {
			if (height == EnhancedSettings::Get(EnhancedSettings::Option::StickerHeight)) {
				return;
			}
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::StickerHeight, height);
		};
		slider->setPseudoDiscrete(
			stickerConstraint.maximum - stickerConstraint.minimum + 1,
			[](int index) { return index; },
			StickerHeightIndexForHeight(currentStickerHeight),
			[=](int index) {
				stickerHeightLabel->fire(
					StickerHeightLabel(StickerHeightForIndex(index)));
			},
			[=](int index) {
				saveStickerHeight(StickerHeightForIndex(index));
			});

		const auto showEmojiButtonAsText = AddButtonWithIcon(
				content,
				tr::lng_settings_show_emoji_button_as_text(),
				st::settingsButtonNoIcon
		);
		registerHighlight(
			EnhancedSettings::Option::ShowEmojiButtonAsText,
			showEmojiButtonAsText);
		showEmojiButtonAsText->toggleOn(
				rpl::single(EnhancedSettings::Get(EnhancedSettings::Option::ShowEmojiButtonAsText))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != EnhancedSettings::Get(EnhancedSettings::Option::ShowEmojiButtonAsText));
		}) | rpl::on_next([=](bool toggled) {
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::ShowEmojiButtonAsText, toggled);
		}, content->lifetime());

		AddDividerText(content, tr::lng_show_emoji_button_as_text_desc());

		const auto showScheduledButton = AddButtonWithIcon(
				content,
				tr::lng_settings_show_scheduled_button(),
				st::settingsButtonNoIcon
		);
		registerHighlight(
			EnhancedSettings::Option::ShowScheduledButton,
			showScheduledButton);
		showScheduledButton->toggleOn(
				rpl::single(EnhancedSettings::Get(EnhancedSettings::Option::ShowScheduledButton))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != EnhancedSettings::Get(EnhancedSettings::Option::ShowScheduledButton));
		}) | rpl::on_next([=](bool toggled) {
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::ShowScheduledButton, toggled);
		}, content->lifetime());

		const auto showPeerId = AddButtonWithIcon(
			content,
			tr::lng_settings_show_peer_id(),
			st::settingsButtonNoIcon
		);
		registerHighlight(
			EnhancedSettings::Option::ShowPeerId,
			showPeerId);
		showPeerId->toggleOn(
				rpl::single(EnhancedSettings::Get(EnhancedSettings::Option::ShowPeerId))
		)->toggledValue(
		) | rpl::filter([](bool enabled) {
			return (enabled != EnhancedSettings::Get(EnhancedSettings::Option::ShowPeerId));
		}) | rpl::on_next([=](bool enabled) {
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::ShowPeerId, enabled);
		}, content->lifetime());

		const auto hideAllChats = AddButtonWithIcon(
			content,
			tr::lng_settings_hide_all_chats(),
			st::settingsAttentionButton
		);
		registerHighlight(
			EnhancedSettings::Option::HideAllChats,
			hideAllChats);
		hideAllChats->toggleOn(
				rpl::single(EnhancedSettings::Get(EnhancedSettings::Option::HideAllChats))
		)->toggledValue(
		) | rpl::filter([](bool enabled) {
			return (enabled != EnhancedSettings::Get(EnhancedSettings::Option::HideAllChats));
		}) | rpl::on_next([=](bool enabled) {
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::HideAllChats, enabled);
		}, content->lifetime());

		const auto hideCounter = AddButtonWithIcon(
				content,
				tr::lng_settings_hide_counter(),
				st::settingsButtonNoIcon
		);
		registerHighlight(
			EnhancedSettings::Option::HideCounter,
			hideCounter);
		hideCounter->toggleOn(
				rpl::single(EnhancedSettings::Get(EnhancedSettings::Option::HideCounter))
		)->toggledValue(
		) | rpl::filter([](bool enabled) {
			return (enabled != EnhancedSettings::Get(EnhancedSettings::Option::HideCounter));
		}) | rpl::on_next([=](bool enabled) {
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::HideCounter, enabled);
		}, content->lifetime());

		const auto hideStories = AddButtonWithIcon(
				content,
				tr::lng_settings_hide_stories(),
				st::settingsButtonNoIcon
		);
		registerHighlight(
			EnhancedSettings::Option::HideStories,
			hideStories);
		hideStories->toggleOn(
				rpl::single(EnhancedSettings::Get(EnhancedSettings::Option::HideStories))
		)->toggledValue(
		) | rpl::filter([](bool enabled) {
			return (enabled != EnhancedSettings::Get(EnhancedSettings::Option::HideStories));
		}) | rpl::on_next([=](bool enabled) {
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::HideStories, enabled);
		}, content->lifetime());

		const auto hideStarRatings = AddButtonWithIcon(
				content,
				tr::lng_settings_hide_star_ratings(),
				st::settingsButtonNoIcon
		);
		registerHighlight(
			EnhancedSettings::Option::HideStarRatings,
			hideStarRatings);
		hideStarRatings->toggleOn(
				rpl::single(EnhancedSettings::Get(EnhancedSettings::Option::HideStarRatings))
		)->toggledValue(
		) | rpl::filter([](bool enabled) {
			return (enabled != EnhancedSettings::Get(EnhancedSettings::Option::HideStarRatings));
		}) | rpl::on_next([=](bool enabled) {
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::HideStarRatings, enabled);
		}, content->lifetime());

		const auto forceMobile = AddButtonWithIcon(
				content,
				tr::lng_settings_force_mobile(),
				st::settingsButtonNoIcon
		);
		registerHighlight(
			EnhancedSettings::Option::ForceMobile,
			forceMobile);
		forceMobile->toggleOn(
				rpl::single(EnhancedSettings::Get(EnhancedSettings::Option::ForceMobile))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != EnhancedSettings::Get(EnhancedSettings::Option::ForceMobile));
		}) | rpl::on_next([=](bool toggled) {
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::ForceMobile, toggled);
		}, content->lifetime());

		const auto hideDeleteForOthersCheckbox = AddButtonWithIcon(
				content,
				tr::lng_settings_hide_delete_for_others_checkbox(),
				st::settingsButtonNoIcon);
		registerHighlight(
			EnhancedSettings::Option::HideDeleteForOthersCheckbox,
			hideDeleteForOthersCheckbox);
		hideDeleteForOthersCheckbox->toggleOn(
			rpl::single(EnhancedSettings::Get(EnhancedSettings::Option::HideDeleteForOthersCheckbox))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return toggled
				!= EnhancedSettings::Get(EnhancedSettings::Option::HideDeleteForOthersCheckbox);
		}) | rpl::on_next([=](bool toggled) {
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::HideDeleteForOthersCheckbox, toggled);
		}, content->lifetime());
	}

	void Enhanced::setupBehavior(not_null<Ui::VerticalLayout*> content) {

		const auto showSimilarOnJoined = AddButtonWithIcon(
				content,
				tr::lng_settings_show_similar_on_joined(),
				st::settingsAttentionButton
		);
		registerHighlight(
			EnhancedSettings::Option::ShowSimilarOnJoined,
			showSimilarOnJoined);
		showSimilarOnJoined->toggleOn(
				rpl::single(EnhancedSettings::Get(EnhancedSettings::Option::ShowSimilarOnJoined))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != EnhancedSettings::Get(EnhancedSettings::Option::ShowSimilarOnJoined));
		}) | rpl::on_next([=](bool toggled) {
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::ShowSimilarOnJoined, toggled);
		}, content->lifetime());

		const auto moreRightActionComments = AddButtonWithIcon(
				content,
				tr::lng_settings_more_right_action_comments(),
				st::settingsAttentionButton
		);
		registerHighlight(
			EnhancedSettings::Option::MoreRightActionComments,
			moreRightActionComments);
		moreRightActionComments->toggleOn(
				rpl::single(EnhancedSettings::Get(EnhancedSettings::Option::MoreRightActionComments))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != EnhancedSettings::Get(EnhancedSettings::Option::MoreRightActionComments));
		}) | rpl::on_next([=](bool toggled) {
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::MoreRightActionComments, toggled);
		}, content->lifetime());

		const auto sendCommentAfterForwarding = AddButtonWithIcon(
				content,
				tr::lng_settings_send_comment_after_forwarding(),
				st::settingsButtonNoIcon);
		registerHighlight(
			EnhancedSettings::Option::SendCommentAfterForwarding,
			sendCommentAfterForwarding);
		sendCommentAfterForwarding->toggleOn(
			rpl::single(EnhancedSettings::Get(EnhancedSettings::Option::SendCommentAfterForwarding))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return toggled
				!= EnhancedSettings::Get(EnhancedSettings::Option::SendCommentAfterForwarding);
		}) | rpl::on_next([=](bool toggled) {
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::SendCommentAfterForwarding, toggled);
		}, content->lifetime());

		const auto disableCloudDraftSync = AddButtonWithIcon(
				content,
				tr::lng_settings_disable_cloud_draft_sync(),
				st::settingsButtonNoIcon
		);
		registerHighlight(
			EnhancedSettings::Option::DisableCloudDraftSync,
			disableCloudDraftSync);
		disableCloudDraftSync->toggleOn(
				rpl::single(EnhancedSettings::Get(EnhancedSettings::Option::DisableCloudDraftSync))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != EnhancedSettings::Get(EnhancedSettings::Option::DisableCloudDraftSync));
		}) | rpl::on_next([=](bool toggled) {
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::DisableCloudDraftSync, toggled);
		}, content->lifetime());

		const auto disableSyncDraftToCloud = AddButtonWithIcon(
				content,
				tr::lng_settings_disable_sync_draft_to_cloud(),
				st::settingsButtonNoIcon);
		registerHighlight(
			EnhancedSettings::Option::DisableSyncDraftToCloud,
			disableSyncDraftToCloud);
		disableSyncDraftToCloud->toggleOn(
			rpl::single(EnhancedSettings::Get(EnhancedSettings::Option::DisableSyncDraftToCloud))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return toggled
				!= EnhancedSettings::Get(EnhancedSettings::Option::DisableSyncDraftToCloud);
		}) | rpl::on_next([=](bool toggled) {
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::DisableSyncDraftToCloud, toggled);
		}, content->lifetime());

		const auto disableLinkWarning = AddButtonWithIcon(
				content,
				tr::lng_settings_disable_link_warning(),
				st::settingsButtonNoIcon
		);
		registerHighlight(
			EnhancedSettings::Option::DisableLinkWarning,
			disableLinkWarning);
		disableLinkWarning->toggleOn(
				rpl::single(EnhancedSettings::Get(EnhancedSettings::Option::DisableLinkWarning))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != EnhancedSettings::Get(EnhancedSettings::Option::DisableLinkWarning));
		}) | rpl::on_next([=](bool toggled) {
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::DisableLinkWarning, toggled);
		}, content->lifetime());

		const auto disableGlobalSearch = AddButtonWithIcon(
				content,
				tr::lng_settings_disable_global_search(),
				st::settingsButtonNoIcon
		);
		registerHighlight(
			EnhancedSettings::Option::DisableGlobalSearch,
			disableGlobalSearch);
		disableGlobalSearch->toggleOn(
				rpl::single(EnhancedSettings::Get(EnhancedSettings::Option::DisableGlobalSearch))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != EnhancedSettings::Get(EnhancedSettings::Option::DisableGlobalSearch));
		}) | rpl::on_next([=](bool toggled) {
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::DisableGlobalSearch, toggled);
		}, content->lifetime());

		const auto extraContextMenu = AddButtonWithIcon(
				content,
				tr::lng_settings_extra_context_menu_options(),
				st::settingsButtonNoIcon
		);
		registerHighlight(
			EnhancedSettings::Option::ExtraContextMenuOptions,
			extraContextMenu);
		extraContextMenu->addClickHandler([=] {
			Ui::show(Box<ExtraContextMenuBox>());
		});

		const auto repeaterSubWrap = content->add(
			object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
				content,
				object_ptr<Ui::VerticalLayout>(content)));
		const auto repeaterContent = repeaterSubWrap->entity();
		repeaterSubWrap->toggle(
			EnhancedSettings::HasExtraContextMenuOption(EnhancedSettings::ExtraContextMenuOption::Repeater),
			anim::type::instant);
		extraContextMenu->events(
		) | rpl::on_next([=](not_null<QEvent*> e) {
			if (e->type() == QEvent::UpdateLater) {
				repeaterSubWrap->toggle(
					EnhancedSettings::HasExtraContextMenuOption(EnhancedSettings::ExtraContextMenuOption::Repeater),
					anim::type::normal);
			}
		}, content->lifetime());

		const auto repeaterReplyToOrig = AddButtonWithIcon(
				repeaterContent,
				tr::lng_settings_repeater_reply_to_orig_msg(),
				st::settingsButtonNoIcon
		);
		registerHighlight(
			EnhancedSettings::Option::RepeaterReplyToOriginal,
			repeaterReplyToOrig);
		repeaterReplyToOrig->toggleOn(
				rpl::single(EnhancedSettings::Get(EnhancedSettings::Option::RepeaterReplyToOriginal))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != EnhancedSettings::Get(EnhancedSettings::Option::RepeaterReplyToOriginal));
		}) | rpl::on_next([=](bool toggled) {
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::RepeaterReplyToOriginal, toggled);
		}, content->lifetime());

		const auto replaceEditButton = AddButtonWithIcon(
				content,
				tr::lng_settings_replace_edit_button(),
				st::settingsButtonNoIcon
		);
		registerHighlight(
			EnhancedSettings::Option::ReplaceEditButton,
			replaceEditButton);
		replaceEditButton->toggleOn(
				rpl::single(EnhancedSettings::Get(EnhancedSettings::Option::ReplaceEditButton))
		)->toggledValue(
		) | rpl::filter([](bool enabled) {
			return (enabled != EnhancedSettings::Get(EnhancedSettings::Option::ReplaceEditButton));
		}) | rpl::on_next([=](bool enabled) {
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::ReplaceEditButton, enabled);
		}, content->lifetime());

		const auto skipMessage = AddButtonWithIcon(
				content,
				tr::lng_settings_skip_message(),
				st::settingsButtonNoIcon
		);
		registerHighlight(
			EnhancedSettings::Option::SkipToNext,
			skipMessage);
		skipMessage->toggleOn(
				rpl::single(EnhancedSettings::Get(EnhancedSettings::Option::SkipToNext))
		)->toggledValue(
		) | rpl::filter([](bool enabled) {
			return (enabled != EnhancedSettings::Get(EnhancedSettings::Option::SkipToNext));
		}) | rpl::on_next([=](bool enabled) {
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::SkipToNext, enabled);
		}, content->lifetime());

		AddDividerText(
			content,
			tr::lng_settings_skip_message_desc());

		const auto communityChatClick = AddButtonWithIcon(
				content,
				tr::lng_settings_community_chat_click(),
				st::settingsButtonNoIcon
		);
		registerHighlight(
			EnhancedSettings::Option::CommunityChatClick,
			communityChatClick);
		communityChatClick->toggleOn(
				rpl::single(EnhancedSettings::Get(EnhancedSettings::Option::CommunityChatClick))
		)->toggledChanges(
		) | rpl::filter([](bool enabled) {
			return (enabled != EnhancedSettings::Get(EnhancedSettings::Option::CommunityChatClick));
		}) | rpl::on_next([=](bool enabled) {
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::CommunityChatClick, enabled);
		}, content->lifetime());

		const auto previewRules = AddButtonWithIcon(
			content,
			tr::lng_link_preview_rules_title(),
			st::settingsButtonNoIcon);
		registerHighlight(
			EnhancedSettings::Option::LinkPreviewRules,
			previewRules);
		previewRules->addClickHandler([=] {
			Ui::show(Box(LinkPreviewRulesBox));
		});
	}

	void Enhanced::setupTranslation(not_null<Ui::VerticalLayout*> content) {
		const auto useGtApi = AddButtonWithIcon(
				content,
				tr::lng_settings_use_gt_api(),
				st::settingsButtonNoIcon
		);
		registerHighlight(
			EnhancedSettings::Option::UseGtApi,
			useGtApi);
		useGtApi->toggleOn(
				rpl::single(EnhancedSettings::Get(EnhancedSettings::Option::UseGtApi))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != EnhancedSettings::Get(EnhancedSettings::Option::UseGtApi));
		}) | rpl::on_next([=](bool toggled) {
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::UseGtApi, toggled);
		}, content->lifetime());

		QString langPackBaseId = Lang::GetInstance().baseId();
		if (langPackBaseId == "zh-hant-raw" || langPackBaseId == "zh-hans-raw") {
			const auto translateToTc = AddButtonWithIcon(
					content,
					tr::lng_settings_translate_to_tc(),
					st::settingsButtonNoIcon
			);
			registerHighlight(
			EnhancedSettings::Option::TranslateToTc,
				translateToTc);
			translateToTc->toggleOn(
					rpl::single(EnhancedSettings::Get(EnhancedSettings::Option::TranslateToTc))
			)->toggledChanges(
			) | rpl::filter([=](bool toggled) {
				return (toggled != EnhancedSettings::Get(EnhancedSettings::Option::TranslateToTc));
			}) | rpl::on_next([=](bool toggled) {
				EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::TranslateToTc, toggled);
			}, content->lifetime());
		}
	}

	void Enhanced::setupVoiceChat(not_null<Ui::VerticalLayout*> page) {
		const auto voiceChatContent = AddEnhancedGroup(
			page,
			tr::lng_settings_voice_chat());

		const auto radioController = AddButtonWithIcon(
				voiceChatContent,
				tr::lng_settings_radio_controller(),
				st::settingsButtonNoIcon
		);
		registerHighlight(
			EnhancedSettings::Option::RadioController,
			radioController);
		radioController->addClickHandler([=] {
			Ui::show(Box<RadioController>());
		});

		AddDividerText(voiceChatContent, tr::lng_radio_controller_desc());

		const auto autoUnmute = AddButtonWithIcon(
				voiceChatContent,
				tr::lng_settings_auto_unmute(),
				st::settingsButtonNoIcon
		);
		registerHighlight(
			EnhancedSettings::Option::AutoUnmute,
			autoUnmute);
		autoUnmute->toggleOn(
				rpl::single(EnhancedSettings::Get(EnhancedSettings::Option::AutoUnmute))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != EnhancedSettings::Get(EnhancedSettings::Option::AutoUnmute));
		}) | rpl::on_next([=](bool toggled) {
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::AutoUnmute, toggled);
		}, voiceChatContent->lifetime());

		AddDividerText(voiceChatContent, tr::lng_auto_unmute_desc());

		const auto enableHdVideo = AddButtonWithIcon(
				voiceChatContent,
				tr::lng_settings_enable_hd_video(),
				st::settingsButtonNoIcon
		);
		registerHighlight(
			EnhancedSettings::Option::HdVideo,
			enableHdVideo);
		enableHdVideo->toggleOn(
				rpl::single(EnhancedSettings::Get(EnhancedSettings::Option::HdVideo))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != EnhancedSettings::Get(EnhancedSettings::Option::HdVideo));
		}) | rpl::on_next([=](bool toggled) {
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::HdVideo, toggled);
		}, voiceChatContent->lifetime());

		auto bitrateValue = rpl::single(
				BitrateController::BitrateLabel(EnhancedSettings::Get(EnhancedSettings::Option::Bitrate))
		) | rpl::then(
				_BitrateChanged.events()
		) | rpl::map([=] {
			return BitrateController::BitrateLabel(EnhancedSettings::Get(EnhancedSettings::Option::Bitrate));
		});

		const auto bitrateController = AddButtonWithLabel(
				page,
				tr::lng_bitrate_controller(),
				std::move(bitrateValue),
				st::settingsButtonNoIcon
		);
		registerHighlight(
			EnhancedSettings::Option::Bitrate,
			bitrateController);
		bitrateController->events(
		) | rpl::on_next([=](not_null<QEvent*> e) {
			const auto event = e->type();
			if (event == QEvent::UpdateLater) _BitrateChanged.fire({});
		}, page->lifetime());
		bitrateController->addClickHandler([=] {
			Ui::show(Box<BitrateController>());
		});

		const auto mprisCallHangup = AddButtonWithIcon(
				page,
				tr::lng_settings_mpris_call_hangup(),
				st::settingsButtonNoIcon
		);
		registerHighlight(
			EnhancedSettings::Option::MprisCallHangup,
			mprisCallHangup);
		mprisCallHangup->toggleOn(
				rpl::single(EnhancedSettings::Get(EnhancedSettings::Option::MprisCallHangup))
		)->toggledValue(
		) | rpl::filter([](bool enabled) {
			return (enabled != EnhancedSettings::Get(EnhancedSettings::Option::MprisCallHangup));
		}) | rpl::on_next([=](bool enabled) {
			EnhancedSettings::ApplyOption(controller(), EnhancedSettings::Option::MprisCallHangup, enabled);
		}, page->lifetime());

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
