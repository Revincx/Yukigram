/*
This file is part of 64Gram Desktop,
the unofficial app based on Telegram Desktop.
For license and copyright information please follow this link:
https://github.com/TDesktop-x64/tdesktop/blob/dev/LEGAL
*/
#pragma once

#include "core/link_preview_rules.h"
#include "rpl/producer.h"

#include <QtCore/QTimer>

namespace EnhancedSettings {

	[[nodiscard]] const Core::LinkPreviewRules &PreviewRules();
	void SetPreviewRules(std::vector<Core::LinkPreviewRule> rules);
	[[nodiscard]] rpl::producer<> PreviewRulesChanges();

	inline constexpr auto kRichMessagePreviewBlocksLimitMin = 5;
	inline constexpr auto kRichMessagePreviewBlocksLimitMax = 50;
	inline constexpr auto kStickerHeightMin = 64;
	inline constexpr auto kStickerHeightMax = 256;

	[[nodiscard]] int RichMessagePreviewBlocksLimit();
	void SetRichMessagePreviewBlocksLimit(int limit);
	[[nodiscard]] int StickerHeight();
	void SetStickerHeight(int height);
	[[nodiscard]] bool AllowScreenshots();
	[[nodiscard]] rpl::producer<bool> AllowScreenshotsValue();
	void SetAllowScreenshots(bool allow);
	[[nodiscard]] bool ShowGroupSenderOnlineStatus();
	[[nodiscard]] rpl::producer<bool> ShowGroupSenderOnlineStatusValue();
	void SetShowGroupSenderOnlineStatus(bool show);

	class Manager : public QObject {
	Q_OBJECT

	public:
		Manager();

		void fill();
		void reset();

		void write(bool force = false);

		void addIdToBlocklist(int64 userId);

		void removeIdFromBlocklist(int64 userId);

		void readBlocklist();

	public Q_SLOTS:

		void writeTimeout();

	private:
		void writeDefaultFile();

		void writeCurrentSettings();

		bool readCustomFile();

		void writing();

		QTimer _jsonWriteTimer;

	};

	void Start();

	void Write();

	void Reset();

	void Finish();

} // namespace EnhancedSettings
