/*
This file is part of 64Gram Desktop,
the unofficial app based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/TDesktop-x64/tdesktop/blob/dev/LEGAL
*/
#pragma once

#include "settings/settings_common_session.h"
#include "settings/settings_type.h"

#include <QPointer>

#include <optional>
#include <vector>

class BoxContent;

namespace Window {
class Controller;
class SessionController;
} // namespace Window

namespace Ui {
class RpWidget;
} // namespace Ui

namespace Settings {

	[[nodiscard]] Type EnhancedId();

	class Enhanced : public Section<Enhanced> {
	public:
		Enhanced(
				QWidget *parent,
				not_null<Window::SessionController *> controller);
		[[nodiscard]] rpl::producer<QString> title() override;
		void fillTopBarMenu(
			const Ui::Menu::MenuCallback &addAction) override;
		void showFinished() override;

	private:
		void setupContent();
		void setupMessages(not_null<Ui::VerticalLayout*> content);
		void setupInterface(not_null<Ui::VerticalLayout*> content);
		void setupBehavior(not_null<Ui::VerticalLayout*> content);
		void setupTranslation(not_null<Ui::VerticalLayout*> content);
		void setupVoiceChat(not_null<Ui::VerticalLayout*> page);
		void setupOther(not_null<Ui::VerticalLayout*> content);
		[[nodiscard]] not_null<Button*> addOptionRow(
			not_null<Ui::VerticalLayout*> content,
			EnhancedSettings::OptionId id,
			std::optional<rpl::producer<QString>> about = std::nullopt);
		void addToggleOption(
			not_null<Ui::VerticalLayout*> content,
			EnhancedSettings::Key<bool> key,
			std::optional<rpl::producer<QString>> about = std::nullopt);
		void addActionOption(
			not_null<Ui::VerticalLayout*> content,
			EnhancedSettings::OptionId id,
			Fn<void()> handler,
			std::optional<rpl::producer<QString>> about = std::nullopt);
		not_null<Button*> addLabeledOption(
			not_null<Ui::VerticalLayout*> content,
			EnhancedSettings::OptionId id,
			rpl::producer<QString> label);
		void addLabeledActionOption(
			not_null<Ui::VerticalLayout*> content,
			EnhancedSettings::OptionId id,
			rpl::producer<QString> label,
			Fn<void()> handler);
		void addIntegerSliderOption(
			not_null<Ui::VerticalLayout*> content,
			EnhancedSettings::Key<int> key,
			Fn<QString(int)> label,
			bool zeroAsMaximum = false);
		template <typename Value>
		void registerHighlight(
				EnhancedSettings::Key<Value> key,
				not_null<Ui::RpWidget*> widget) {
			registerHighlight(key.id, widget);
		}
		void registerHighlight(
			EnhancedSettings::OptionId id,
			not_null<Ui::RpWidget*> widget);
		void registerHighlight(
			QString id,
			not_null<Ui::RpWidget*> widget);
		void registerHighlight(
			QString id,
			std::optional<EnhancedSettings::OptionId> option,
			not_null<Ui::RpWidget*> widget);

		std::vector<std::pair<QString, QPointer<QWidget>>> _highlightControls;

	};

} // namespace Settings
