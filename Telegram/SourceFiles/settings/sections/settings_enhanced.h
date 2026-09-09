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
		void showFinished() override;

	private:
		void setupContent();
		void setupMessages(not_null<Ui::VerticalLayout*> content);
		void setupInterface(not_null<Ui::VerticalLayout*> content);
		void setupBehavior(not_null<Ui::VerticalLayout*> content);
		void setupTranslation(not_null<Ui::VerticalLayout*> content);
		void setupVoiceChat(not_null<Ui::VerticalLayout*> page);
		void setupOther(not_null<Ui::VerticalLayout*> content);
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

		rpl::event_stream<QString> _AlwaysDeleteChanged;
		rpl::event_stream<QString> _BitrateChanged;
		rpl::event_stream<> _RichMessagePreviewBlocksChanged;

		std::vector<std::pair<QString, QPointer<QWidget>>> _highlightControls;

	};

} // namespace Settings
