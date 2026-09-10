/*
This file is part of Yurigram Desktop,
the unofficial app based on Telegram Desktop.
For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "core/chat_enhanced_settings.h"

class PeerData;

namespace Window {
class SessionController;
} // namespace Window

namespace Settings {

[[nodiscard]] bool HasChatEnhancedSettings(not_null<PeerData*> peer);
[[nodiscard]] bool HasChatEnhancedFeature(
	not_null<PeerData*> peer,
	EnhancedSettings::ChatFeature feature);
void ShowChatEnhancedSettings(
	not_null<Window::SessionController*> controller,
	not_null<PeerData*> peer,
	std::optional<EnhancedSettings::ChatFeature> feature = std::nullopt);

} // namespace Settings
