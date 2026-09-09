#include "boxes/link_preview_rules_box.h"

#include "core/enhanced_settings.h"
#include "lang/lang_keys.h"
#include "settings/settings_common.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/shadow.h"

#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"

namespace {

void EditRuleBox(not_null<Ui::GenericBox*> box, int index) {
	const auto &rules = EnhancedSettings::PreviewRules().rules();
	const auto editing = (index >= 0 && index < int(rules.size()));
	const auto initial = editing ? rules[index] : Core::LinkPreviewRule();
	box->setTitle(editing
		? tr::lng_link_preview_rule_edit()
		: tr::lng_link_preview_rule_add());
	const auto pattern = box->addRow(object_ptr<Ui::InputField>(
		box,
		st::defaultInputField,
		tr::lng_link_preview_rule_pattern(),
		initial.urlPattern));
	const auto domain = box->addRow(object_ptr<Ui::InputField>(
		box,
		st::defaultInputField,
		tr::lng_link_preview_rule_domain(),
		initial.replacementDomain));
	box->setFocusCallback([=] {
		pattern->setFocusFast();
	});
	const auto save = [=] {
		const auto expression = pattern->getLastText();
		const auto target = Core::LinkPreviewRules::NormalizeDomain(
			domain->getLastText());
		const auto validPattern = Core::LinkPreviewRules::ValidPattern(
			expression);
		if (!validPattern) {
			pattern->showErrorNoFocus();
		}
		if (target.isEmpty()) {
			domain->showErrorNoFocus();
		}
		if (!validPattern || target.isEmpty()) {
			(validPattern ? domain : pattern)->setFocusFast();
			return;
		}
		auto updated = EnhancedSettings::PreviewRules().rules();
		if (editing) {
			if (index >= int(updated.size()) || updated[index] != initial) {
				box->closeBox();
				return;
			}
			updated[index] = { expression, target };
		} else {
			updated.push_back({ expression, target });
		}
		EnhancedSettings::SetPreviewRules(std::move(updated));
		EnhancedSettings::Write();
		box->closeBox();
	};
	pattern->changes() | rpl::on_next([=] {
		pattern->hideError();
	}, pattern->lifetime());
	domain->changes() | rpl::on_next([=] {
		domain->hideError();
	}, domain->lifetime());
	pattern->submits() | rpl::on_next([=] {
		domain->setFocusFast();
	}, pattern->lifetime());
	domain->submits() | rpl::on_next(save, domain->lifetime());
	box->addButton(tr::lng_settings_save(), save);
	box->addButton(tr::lng_cancel(), [=] {
		box->closeBox();
	});
	if (editing) {
		box->addLeftButton(tr::lng_box_delete(), [=] {
			auto updated = EnhancedSettings::PreviewRules().rules();
			if (index < int(updated.size()) && updated[index] == initial) {
				updated.erase(updated.begin() + index);
				EnhancedSettings::SetPreviewRules(std::move(updated));
				EnhancedSettings::Write();
			}
			box->closeBox();
		});
	}
}

not_null<Settings::Button*> AddRuleLine(
		not_null<Ui::VerticalLayout*> list,
		const Core::LinkPreviewRule &rule) {
	const auto button = Settings::AddButtonWithIcon(
		list,
		rpl::single(rule.urlPattern),
		st::settingsButtonNoIcon);
	const auto domain = Ui::CreateChild<Ui::FlatLabel>(
		button.get(),
		rule.replacementDomain,
		st::settingsButtonNoIcon.rightLabel);
	domain->show();
	button->widthValue(
	) | rpl::on_next([=](int width) {
		const auto &style = st::settingsButtonNoIcon;
		const auto available = std::max(
			width
				- style.padding.left()
				- st::settingsButtonRightSkip
				- st::defaultVerticalListSkip,
			0);
		domain->resizeToNaturalWidth(available);
		domain->moveToRight(
			st::settingsButtonRightSkip,
			style.padding.top());
		auto padding = style.padding;
		padding.setRight(
			st::settingsButtonRightSkip
				+ domain->width()
				+ st::defaultVerticalListSkip);
		button->setPaddingOverride(padding);
	}, domain->lifetime());
	domain->setAttribute(Qt::WA_TransparentForMouseEvents);
	return button;
}

void FillRules(
		not_null<Ui::GenericBox*> box,
		not_null<Ui::VerticalLayout*> list) {
	list->clear();
	const auto &rules = EnhancedSettings::PreviewRules().rules();
	if (rules.empty()) {
		list->add(
			object_ptr<Ui::FlatLabel>(
				list,
				tr::lng_link_preview_rules_empty(),
				st::settingsSearchNoResults),
			st::settingsSearchNoResultsPadding,
			style::al_justify);
		return;
	}
	for (auto i = 0; i != int(rules.size()); ++i) {
		if (i) {
			list->add(
				object_ptr<Ui::PlainShadow>(
					list,
					st::menuSeparatorFg),
				QMargins(
					st::settingsButtonNoIcon.padding.left(),
					0,
					st::settingsButtonRightSkip,
					0));
		}
		AddRuleLine(list, rules[i])->setClickedCallback([=] {
			box->getDelegate()->show(Box(EditRuleBox, i));
		});
	}
}

} // namespace

void LinkPreviewRulesBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_link_preview_rules_title());
	box->setWidth(st::boxWideWidth);
	box->setMaxHeight(st::boxMaxListHeight);
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		tr::lng_link_preview_rules_about(),
		st::boxLabel));
	box->addSkip(st::defaultVerticalListSkip);
	const auto list = box->addRow(
		object_ptr<Ui::VerticalLayout>(box),
		style::margins());
	FillRules(box, list);
	EnhancedSettings::PreviewRulesChanges() | rpl::on_next([=] {
		FillRules(box, list);
	}, box->lifetime());
	box->addLeftButton(tr::lng_link_preview_rule_add(), [=] {
		box->getDelegate()->show(Box(EditRuleBox, -1));
	});
	box->addButton(tr::lng_box_done(), [=] {
		box->closeBox();
	});
}
