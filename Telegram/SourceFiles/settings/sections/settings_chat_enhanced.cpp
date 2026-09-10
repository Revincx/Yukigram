/*
This file is part of Yurigram Desktop,
the unofficial app based on Telegram Desktop.
For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "settings/sections/settings_chat_enhanced.h"

#include "core/chat_enhanced_settings.h"
#include "data/data_chat_participant_status.h"
#include "data/data_peer.h"
#include "history/view/history_view_chat_preview.h"
#include "info/profile/info_profile_values.h"
#include "lang/lang_keys.h"
#include "settings/settings_common_session.h"
#include "ui/boxes/confirm_box.h"
#include "ui/boxes/single_choice_box.h"
#include "ui/controls/userpic_button.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/ui_utility.h"
#include "ui/vertical_list.h"
#include "ui/toast/toast.h"
#include "window/window_session_controller.h"

#include <array>
#include <memory>
#include <optional>
#include <rpl/variable.h>
#include <utility>
#include <vector>

#include "styles/style_chat.h"
#include "styles/style_chat_helpers.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"
#include "styles/style_window.h"

namespace Settings {
namespace {

using Feature = EnhancedSettings::ChatFeature;
using Override = EnhancedSettings::ChatFeatureOverride;
using TitleProducer = rpl::producer<QString> (*)();
using Available = bool (*)(not_null<PeerData*>);

enum class Group {
	Messages,
	Interface,
	Behavior,
	Count,
};

struct FeatureDescriptor {
	Feature feature = Feature::Count;
	Group group = Group::Count;
	TitleProducer title = nullptr;
	Available available = nullptr;
};

class ChatEnhancedSection;

class ChatEnhancedFactory final
	: public AbstractSectionFactory
	, public std::enable_shared_from_this<ChatEnhancedFactory> {
public:
	explicit ChatEnhancedFactory(not_null<PeerData*> peer);

	object_ptr<AbstractSection> create(
		not_null<QWidget*> parent,
		not_null<Window::SessionController*> controller,
		not_null<Ui::ScrollArea*>,
		rpl::producer<Container>) const override;

private:
	const not_null<PeerData*> _peer;

};

class ChatEnhancedSection final : public AbstractSection {
public:
	ChatEnhancedSection(
		QWidget *parent,
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> peer,
		Type id);

	[[nodiscard]] Type id() const override;
	[[nodiscard]] rpl::producer<QString> title() override;
	[[nodiscard]] bool centerLayerVertically() const override;
	[[nodiscard]] base::unique_qptr<Ui::RpWidget> createTopBarButton(
		QWidget *parent,
		bool layer) override;

private:
	void setupContent();

	const Type _id;
	const not_null<PeerData*> _peer;
	std::array<
		std::shared_ptr<rpl::variable<Override>>,
		static_cast<std::size_t>(Feature::Count)> _values;

};

ChatEnhancedFactory::ChatEnhancedFactory(not_null<PeerData*> peer)
: _peer(peer) {
}

object_ptr<AbstractSection> ChatEnhancedFactory::create(
		not_null<QWidget*> parent,
		not_null<Window::SessionController*> controller,
		not_null<Ui::ScrollArea*>,
		rpl::producer<Container>) const {
	const auto id = std::const_pointer_cast<ChatEnhancedFactory>(
		shared_from_this());
	return object_ptr<ChatEnhancedSection>(
		parent,
		controller,
		_peer,
		id);
}

ChatEnhancedSection::ChatEnhancedSection(
	QWidget *parent,
	not_null<Window::SessionController*> controller,
	not_null<PeerData*> peer,
	Type id)
: AbstractSection(parent, controller)
, _id(std::move(id))
, _peer(peer) {
	setupContent();
}

Type ChatEnhancedSection::id() const {
	return _id;
}

rpl::producer<QString> ChatEnhancedSection::title() {
	return tr::lng_chat_enhanced_settings();
}

bool ChatEnhancedSection::centerLayerVertically() const {
	return true;
}

base::unique_qptr<Ui::RpWidget> ChatEnhancedSection::createTopBarButton(
		QWidget *parent,
		bool layer) {
	auto button = base::make_unique_q<Ui::IconButton>(
		parent,
		layer
			? st::settingsChatEnhancedLayerTopBarReset
			: st::settingsChatEnhancedTopBarReset);
	button->setAccessibleName(tr::lng_chat_enhanced_reset(tr::now));
	button->addClickHandler([=] {
		controller()->show(Ui::MakeConfirmBox({
			.text = tr::lng_chat_enhanced_reset_sure(),
			.confirmed = crl::guard(this, [=](Fn<void()> &&close) {
				EnhancedSettings::ResetChatFeatureOverrides(_peer);
				for (const auto &value : _values) {
					if (value) {
						(*value) = Override::Default;
					}
				}
				close();
				controller()->showToast({
					.text = { tr::lng_chat_enhanced_reset_done(tr::now) },
					.iconLottie = u"toast/contact_check"_q,
					.iconLottieSize = st::toastLottieIconSize,
				});
			}),
			.confirmText = tr::lng_background_reset_default(),
			.confirmStyle = &st::attentionBoxButton,
		}));
	});
	return base::unique_qptr<Ui::RpWidget>(std::move(button));
}

rpl::producer<QString> ForceShowWebPagePreviewTitle() {
	return tr::lng_settings_force_show_webpage_preview();
}

rpl::producer<QString> DisableAutoFetchWebPagePreviewTitle() {
	return tr::lng_settings_disable_auto_fetch_webpage_preview();
}

rpl::producer<QString> RemoveMediaSpoilerTitle() {
	return tr::lng_settings_remove_media_spoiler();
}

rpl::producer<QString> HideBlockedMessagesTitle() {
	return tr::lng_settings_hide_messages();
}

rpl::producer<QString> ShowScheduledButtonTitle() {
	return tr::lng_settings_show_scheduled_button();
}

rpl::producer<QString> DisableCloudDraftSyncTitle() {
	return tr::lng_settings_disable_cloud_draft_sync();
}

rpl::producer<QString> DisableSyncDraftToCloudTitle() {
	return tr::lng_settings_disable_sync_draft_to_cloud();
}

bool ForceShowWebPagePreviewAvailable(not_null<PeerData*> peer) {
	return !peer->isSelf();
}

bool DisableAutoFetchWebPagePreviewAvailable(not_null<PeerData*> peer) {
	return Data::CanSendTexts(peer);
}

bool RemoveMediaSpoilerAvailable(not_null<PeerData*> peer) {
	return !peer->isSelf();
}

bool HideBlockedMessagesAvailable(not_null<PeerData*> peer) {
	return peer->isChat() || peer->isMegagroup();
}

bool ShowScheduledButtonAvailable(not_null<PeerData*> peer) {
	const auto rights = Data::AllSendRestrictions()
		& ~ChatRestriction::SendPolls;
	return !peer->starsPerMessageChecked()
		&& Data::CanSendAnyOf(peer, rights, false);
}

bool DraftSyncAvailable(not_null<PeerData*> peer) {
	return Data::CanSendAnything(peer, false);
}

constexpr auto kFeatureDescriptors = std::array{
	FeatureDescriptor{
		.feature = Feature::ForceShowWebPagePreview,
		.group = Group::Messages,
		.title = ForceShowWebPagePreviewTitle,
		.available = ForceShowWebPagePreviewAvailable,
	},
	FeatureDescriptor{
		.feature = Feature::DisableAutoFetchWebPagePreview,
		.group = Group::Messages,
		.title = DisableAutoFetchWebPagePreviewTitle,
		.available = DisableAutoFetchWebPagePreviewAvailable,
	},
	FeatureDescriptor{
		.feature = Feature::RemoveMediaSpoiler,
		.group = Group::Messages,
		.title = RemoveMediaSpoilerTitle,
		.available = RemoveMediaSpoilerAvailable,
	},
	FeatureDescriptor{
		.feature = Feature::HideBlockedMessages,
		.group = Group::Messages,
		.title = HideBlockedMessagesTitle,
		.available = HideBlockedMessagesAvailable,
	},
	FeatureDescriptor{
		.feature = Feature::ShowScheduledButton,
		.group = Group::Interface,
		.title = ShowScheduledButtonTitle,
		.available = ShowScheduledButtonAvailable,
	},
	FeatureDescriptor{
		.feature = Feature::DisableCloudDraftSync,
		.group = Group::Behavior,
		.title = DisableCloudDraftSyncTitle,
		.available = DraftSyncAvailable,
	},
	FeatureDescriptor{
		.feature = Feature::DisableSyncDraftToCloud,
		.group = Group::Behavior,
		.title = DisableSyncDraftToCloudTitle,
		.available = DraftSyncAvailable,
	},
};
static_assert(
	kFeatureDescriptors.size()
	== static_cast<std::size_t>(Feature::Count));

constexpr auto kGroups = std::array{
	Group::Messages,
	Group::Interface,
	Group::Behavior,
};
static_assert(
	kGroups.size()
	== static_cast<std::size_t>(Group::Count));

rpl::producer<QString> GroupTitle(Group group) {
	switch (group) {
	case Group::Messages:
		return tr::lng_settings_messages();
	case Group::Interface:
		return tr::lng_settings_interface();
	case Group::Behavior:
		return tr::lng_settings_behavior();
	case Group::Count:
		break;
	}
	Unexpected("Unknown chat enhanced settings group.");
}

int OverrideIndex(Override value) {
	switch (value) {
	case Override::Default:
		return 0;
	case Override::Enabled:
		return 1;
	case Override::Disabled:
		return 2;
	}
	Unexpected("Unknown ChatFeatureOverride.");
}

Override OverrideFromIndex(int index) {
	switch (index) {
	case 0:
		return Override::Default;
	case 1:
		return Override::Enabled;
	case 2:
		return Override::Disabled;
	}
	Unexpected("Unknown chat feature override index.");
}

rpl::producer<QString> OverrideLabel(rpl::producer<Override> value) {
	return rpl::combine(
		std::move(value),
		tr::lng_chat_enhanced_default(),
		tr::lng_chat_enhanced_enabled(),
		tr::lng_chat_enhanced_disabled()
	) | rpl::map([](
			Override value,
			QString defaultLabel,
			QString enabledLabel,
			QString disabledLabel) {
		switch (value) {
		case Override::Default:
			return defaultLabel;
		case Override::Enabled:
			return enabledLabel;
		case Override::Disabled:
			return disabledLabel;
		}
		Unexpected("Unknown ChatFeatureOverride.");
	});
}

bool IsAvailable(
		not_null<PeerData*> peer,
		const FeatureDescriptor &descriptor) {
	return descriptor.available && descriptor.available(peer);
}

bool HasGroup(not_null<PeerData*> peer, Group group) {
	for (const auto &descriptor : kFeatureDescriptors) {
		if (descriptor.group == group && IsAvailable(peer, descriptor)) {
			return true;
		}
	}
	return false;
}

void ShowOverrideBox(
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> peer,
		FeatureDescriptor descriptor,
		std::shared_ptr<rpl::variable<Override>> value) {
	Expects(descriptor.title != nullptr);
	const auto feature = descriptor.feature;
	const auto title = descriptor.title;
	const auto options = std::make_shared<std::vector<QString>>(
		std::vector{
			tr::lng_chat_enhanced_default(tr::now),
			tr::lng_chat_enhanced_enabled(tr::now),
			tr::lng_chat_enhanced_disabled(tr::now),
		});
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		SingleChoiceBox(box, {
			.title = title(),
			.options = *options,
			.initialSelection = OverrideIndex(value->current()),
			.callback = [=](int index) {
				const auto selected = OverrideFromIndex(index);
				EnhancedSettings::SetChatFeatureOverride(
					peer,
					feature,
					selected);
				(*value) = selected;
			},
		});
	}));
}

void AddFeature(
		not_null<Window::SessionController*> controller,
		not_null<Ui::VerticalLayout*> container,
		not_null<PeerData*> peer,
		FeatureDescriptor descriptor,
		std::shared_ptr<rpl::variable<Override>> value) {
	Expects(descriptor.title != nullptr);
	const auto button = AddButtonWithLabel(
		container,
		descriptor.title(),
		OverrideLabel(value->value()),
		st::settingsButtonNoIcon);
	button->addClickHandler([=] {
		ShowOverrideBox(controller, peer, descriptor, value);
	});
}

void ChatEnhancedSection::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	const auto labels = Ui::CreateChild<Ui::VerticalLayout>(content);
	labels->add(object_ptr<Ui::FlatLabel>(
		labels,
		Info::Profile::NameValue(_peer),
		st::previewName));
	auto statusFields = HistoryView::ChatStatusValue(
		_peer
	) | rpl::start_spawning(lifetime());
	const auto subtitle = labels->add(
		object_ptr<Ui::FlatLabel>(
			labels,
			rpl::duplicate(statusFields) | rpl::map(
				[](HistoryView::ChatStatus fields) {
					return std::move(fields.text);
				}),
			st::previewStatus));
	std::move(statusFields) | rpl::on_next(
		[=](const HistoryView::ChatStatus &fields) {
			subtitle->setTextColorOverride(fields.active
				? st::windowActiveTextFg->c
				: std::optional<QColor>());
		}, subtitle->lifetime());

	Ui::AddDivider(content);
	Ui::AddSkip(content);
	Ui::IconWithTitle(
		content,
		Ui::CreateChild<Ui::UserpicButton>(
			content,
			_peer,
			st::mainMenuUserpic,
			Ui::PeerUserpicShape::Circle),
		labels);
	Ui::AddSkip(content);

	Ui::AddDividerText(content, tr::lng_chat_enhanced_settings_desc());

	auto firstGroup = true;
	for (const auto group : kGroups) {
		if (!HasGroup(_peer, group)) {
			continue;
		}
		if (!firstGroup) {
			Ui::AddDivider(content);
			Ui::AddSkip(content);
		}
		Ui::AddSubsectionTitle(content, GroupTitle(group));
		const auto inner = content->add(
			object_ptr<Ui::VerticalLayout>(content));
		for (const auto &descriptor : kFeatureDescriptors) {
			if (descriptor.group == group
				&& IsAvailable(_peer, descriptor)) {
				const auto index = static_cast<std::size_t>(
					descriptor.feature);
				auto &value = _values[index];
				value = std::make_shared<rpl::variable<Override>>(
					EnhancedSettings::GetChatFeatureOverride(
						_peer,
						descriptor.feature));
				AddFeature(
					controller(),
					inner,
					_peer,
					descriptor,
					value);
			}
		}
		Ui::AddSkip(content);
		firstGroup = false;
	}

	Ui::ResizeFitChild(this, content);
}

} // namespace

bool HasChatEnhancedSettings(not_null<PeerData*> peer) {
	for (const auto &descriptor : kFeatureDescriptors) {
		if (IsAvailable(peer, descriptor)) {
			return true;
		}
	}
	return false;
}

void ShowChatEnhancedSettings(
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> peer) {
	controller->showSettings(std::make_shared<ChatEnhancedFactory>(peer));
}

} // namespace Settings
