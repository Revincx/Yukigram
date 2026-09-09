/*
This file is part of 64Gram Desktop,
the unofficial app based on Telegram Desktop.
For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "boxes/enhanced_options_box.h"

#include "core/enhanced_settings.h"
#include "data/data_histories.h"
#include "data/data_session.h"
#include <facades.h>
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "mainwindow.h"
#include "settings/sections/settings_enhanced.h"
#include <ui/toast/toast.h>
#include "ui/widgets/checkbox.h"
#include "ui/widgets/continuous_sliders.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "window/window_session_controller.h"

#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "styles/style_passcode_box.h"
#include "styles/style_edit_peer_members.h"
#include "styles/style_settings_local_storage.h"

#include <algorithm>

namespace {

constexpr auto kRichMessagePreviewDefaultGapSections = 5;

[[nodiscard]] EnhancedSettings::IntegerConstraint RichMessageConstraint() {
	return EnhancedSettings::IntegerConstraintFor(
		EnhancedSettings::Option::RichMessagePreviewBlocksLimit);
}

[[nodiscard]] int RichMessageMaxIndex() {
	const auto constraint = RichMessageConstraint();
	return constraint.maximum - constraint.minimum;
}

[[nodiscard]] int RichMessageDefaultIndex() {
	return RichMessageMaxIndex() + kRichMessagePreviewDefaultGapSections;
}

[[nodiscard]] int RichMessagePreviewLimitForIndex(int index) {
	const auto constraint = RichMessageConstraint();
	return (index == RichMessageDefaultIndex())
		? 0
		: (constraint.minimum + std::min(index, RichMessageMaxIndex()));
}

[[nodiscard]] int RichMessagePreviewIndexForLimit(int limit) {
	return limit
		? (limit - RichMessageConstraint().minimum)
		: RichMessageDefaultIndex();
}

[[nodiscard]] QString RichMessagePreviewBlocksLabel(int limit) {
	return tr::lng_settings_rich_message_preview_blocks_count(
		tr::now,
		lt_count,
		limit);
}

} // namespace

ExtraContextMenuBox::ExtraContextMenuBox(QWidget *parent) {
}

void ExtraContextMenuBox::prepare() {
	setTitle(tr::lng_settings_extra_context_menu_options());

	addButton(tr::lng_box_ok(), [=] { closeBox(); });

	auto y = st::boxOptionListPadding.top() + st::boxMediumSkip;

	struct OptionEntry {
		EnhancedSettings::ExtraContextMenuOption value;
		QString label;
	};
	const auto options = std::vector<OptionEntry>{
		{ EnhancedSettings::ExtraContextMenuOption::Repeater, tr::lng_context_repeater(tr::now) },
		{ EnhancedSettings::ExtraContextMenuOption::MoreForward, tr::lng_context_more_forward(tr::now) },
		{ EnhancedSettings::ExtraContextMenuOption::HideMessage, tr::lng_context_hide_message(tr::now) },
		{ EnhancedSettings::ExtraContextMenuOption::ViewAsJson, tr::lng_context_view_as_json(tr::now) },
	};

	for (const auto &[optValue, label] : options) {
		const auto checked = EnhancedSettings::HasExtraContextMenuOption(optValue);
		const auto checkbox = Ui::CreateChild<Ui::Checkbox>(
			this,
			label,
			checked);
		checkbox->moveToLeft(st::boxPadding.left(), y);
		y += checkbox->heightNoMargins() + st::boxOptionListSkip;
		const auto optInt = static_cast<int>(optValue);
		checkbox->checkedChanges(
		) | rpl::filter([=](bool isChecked) {
			return isChecked != EnhancedSettings::HasExtraContextMenuOption(
				static_cast<EnhancedSettings::ExtraContextMenuOption>(optInt));
		}) | rpl::on_next([=](bool isChecked) {
			auto list = EnhancedSettings::Get(EnhancedSettings::Option::ExtraContextMenuOptions);
			if (isChecked && !list.contains(optInt)) {
				list.append(optInt);
			} else if (!isChecked) {
				list.removeAll(optInt);
			}
			EnhancedSettings::Set(
				EnhancedSettings::Option::ExtraContextMenuOptions,
				std::move(list));
		}, lifetime());
	}

	showChildren();
	setDimensions(st::boxWidth, y);
}

RadioController::RadioController(QWidget *parent)
		: _url(this, st::defaultInputField, tr::lng_formatting_link_url()) {
}

void RadioController::prepare() {
	setTitle(tr::lng_settings_radio_controller());

	addButton(tr::lng_settings_save(), [=] { save(); });
	addButton(tr::lng_cancel(), [=] { closeBox(); });

	_url->setText(EnhancedSettings::Get(EnhancedSettings::Option::RadioController));

	setDimensions(st::boxWidth, _url->height());
}

void RadioController::setInnerFocus() {
	_url->setFocusFast();
}

void RadioController::resizeEvent(QResizeEvent *e) {
	BoxContent::resizeEvent(e);

	int32 w = st::boxWidth - st::boxPadding.left() - st::boxPadding.right();
	_url->resize(w, _url->height());
	_url->moveToLeft(st::boxPadding.left(), 0);
}

void RadioController::save() {
	auto host = _url->getLastText().trimmed();
	if (host == "") {
		host = "http://localhost:2468";
	}
	EnhancedSettings::Set(EnhancedSettings::Option::RadioController, host);
	closeBox();
}

BitrateController::BitrateController(QWidget *parent) {
}

void BitrateController::prepare() {
	setTitle(tr::lng_bitrate_controller());

	addButton(tr::lng_settings_save(), [=] { save(); });
	addButton(tr::lng_cancel(), [=] { closeBox(); });

	auto y = st::boxOptionListPadding.top();
	_description.create(
			this,
			tr::lng_bitrate_controller_desc(tr::now),
			st::boxLabel);
	_description->moveToLeft(st::boxPadding.left(), y);
	_description->resizeToWidth(st::boxWidth - st::boxPadding.left() - st::boxPadding.right());

	y += _description->height() + st::boxMediumSkip;

	_bitrateGroup = std::make_shared<Ui::RadiobuttonGroup>(EnhancedSettings::Get(EnhancedSettings::Option::Bitrate));

	for (int i = 0; i <= 7; i++) {
		const auto button = Ui::CreateChild<Ui::Radiobutton>(
				this,
				_bitrateGroup,
				i,
				BitrateLabel(i),
				st::autolockButton);
		button->moveToLeft(st::boxPadding.left(), y);
		y += button->heightNoMargins() + st::boxOptionListSkip;
	}
	showChildren();
	setDimensions(st::boxWidth, y);
}

QString BitrateController::BitrateLabel(int boost) {
	switch (boost) {
		case 0:
			return tr::lng_bitrate_controller_default(tr::now);
		case 1:
			return tr::lng_bitrate_controller_64k(tr::now);
		case 2:
			return tr::lng_bitrate_controller_96k(tr::now);
		case 3:
			return tr::lng_bitrate_controller_128k(tr::now);
		case 4:
			return tr::lng_bitrate_controller_160k(tr::now);
		case 5:
			return tr::lng_bitrate_controller_192k(tr::now);
		case 6:
			return tr::lng_bitrate_controller_256k(tr::now);
		case 7:
			return tr::lng_bitrate_controller_320k(tr::now);
		default:
			Unexpected("Bitrate not found.");
	}
}

void BitrateController::save() {
	EnhancedSettings::ApplyOption(
		App::wnd()->sessionController(),
		EnhancedSettings::Option::Bitrate,
		_bitrateGroup->current());
	closeBox();
}

RichMessagePreviewBlocksBox::RichMessagePreviewBlocksBox(QWidget *parent) {
}

void RichMessagePreviewBlocksBox::prepare() {
	setTitle(tr::lng_settings_rich_message_preview_blocks());

	addButton(tr::lng_settings_save(), [=] { save(); });
	addButton(tr::lng_cancel(), [=] { closeBox(); });

	auto y = st::boxOptionListPadding.top();
	_description.create(
		this,
		tr::lng_settings_rich_message_preview_blocks_desc(tr::now),
		st::boxLabel);
	_description->moveToLeft(st::boxPadding.left(), y);
	_description->resizeToWidth(
		st::boxWidth - st::boxPadding.left() - st::boxPadding.right());
	y += _description->height() + st::boxMediumSkip;

	_limit = EnhancedSettings::Get(EnhancedSettings::Option::RichMessagePreviewBlocksLimit);
	_current.create(
		this,
		_limit
			? RichMessagePreviewBlocksLabel(_limit)
			: tr::lng_font_default(tr::now),
		st::richMessagePreviewBlocksCurrent);
	_labelsTop = y;
	updateCurrentLabel();
	y += _current->height() + st::boxMediumSkip;

	_slider.create(this, st::localStorageLimitSlider);
	const auto sliderWidth
		= st::boxWidth - st::boxPadding.left() - st::boxPadding.right();
	_slider->resize(
		sliderWidth,
		st::localStorageLimitSlider.seekSize.height());
	_slider->moveToLeft(st::boxPadding.left(), y);

	_slider->setPseudoDiscrete(
		RichMessageDefaultIndex() + 1,
		[](int index) { return index; },
		RichMessagePreviewIndexForLimit(_limit),
		[=](int index) {
			_limit = RichMessagePreviewLimitForIndex(index);
			updateCurrentLabel();
		});
	const auto constraint = RichMessageConstraint();
	for (const auto limit : {
		constraint.minimum,
		15,
		25,
		35,
		constraint.maximum,
	}) {
		const auto progress = (limit
			- constraint.minimum)
			/ float64(RichMessageDefaultIndex());
		_slider->addDivider(
			progress,
			st::richMessagePreviewBlocksDivider);
	}
	_slider->addDivider(
		(RichMessageMaxIndex() + 1.) / RichMessageDefaultIndex(),
		st::richMessagePreviewBlocksDefaultDivider);

	y += _slider->height() + st::richMessagePreviewBlocksTickSkip;
	const auto minimum = Ui::CreateChild<Ui::FlatLabel>(
		this,
		QString::number(constraint.minimum),
		st::richMessagePreviewBlocksTick);
	const auto defaultLabel = Ui::CreateChild<Ui::FlatLabel>(
		this,
		tr::lng_font_default(tr::now),
		st::richMessagePreviewBlocksTick);
	minimum->moveToLeft(st::boxPadding.left(), y);
	defaultLabel->moveToRight(st::boxPadding.right(), y, st::boxWidth);
	for (const auto limit : { 15, 25, 35, 50 }) {
		const auto label = Ui::CreateChild<Ui::FlatLabel>(
			this,
			QString::number(limit),
			st::richMessagePreviewBlocksTick);
		const auto progress = (limit
			- constraint.minimum)
			/ float64(RichMessageDefaultIndex());
		const auto position = st::boxPadding.left()
			+ (st::localStorageLimitSlider.seekSize.width() / 2)
			+ base::SafeRound(progress * (sliderWidth
				- st::localStorageLimitSlider.seekSize.width()));
		label->moveToLeft(position - label->width() / 2, y);
	}
	y += std::max(minimum->height(), defaultLabel->height())
		+ st::boxOptionListPadding.bottom();
	showChildren();
	setDimensions(st::boxWidth, y);
}

void RichMessagePreviewBlocksBox::updateCurrentLabel() {
	_current->setText(_limit
		? RichMessagePreviewBlocksLabel(_limit)
		: tr::lng_font_default(tr::now));
	_current->moveToLeft((st::boxWidth - _current->width()) / 2, _labelsTop);
}

void RichMessagePreviewBlocksBox::save() {
	EnhancedSettings::ApplyOption(
		App::wnd()->sessionController(),
		EnhancedSettings::Option::RichMessagePreviewBlocksLimit,
		_limit);
	closeBox();
}
