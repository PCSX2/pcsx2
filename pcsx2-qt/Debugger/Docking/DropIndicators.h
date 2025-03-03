// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <kddockwidgets/core/indicators/ClassicDropIndicatorOverlay.h>
#include <kddockwidgets/core/views/ClassicIndicatorWindowViewInterface.h>
#include <kddockwidgets/qtwidgets/views/SegmentedDropIndicatorOverlay.h>

class DockDropIndicator;

// This switches between our custom drop indicators and KDDockWidget's built-in
// ones on the fly depending on whether or not we have a windowing system that
// supports compositing.
class DockDropIndicatorProxy : public KDDockWidgets::Core::ClassicIndicatorWindowViewInterface
{
public:
	DockDropIndicatorProxy(KDDockWidgets::Core::ClassicDropIndicatorOverlay* classic_indicators);
	~DockDropIndicatorProxy();

	void setObjectName(const QString&) override;
	KDDockWidgets::DropLocation hover(QPoint globalPos) override;
	QPoint posForIndicator(KDDockWidgets::DropLocation) const override;
	void updatePositions() override;
	void raise() override;
	void setVisible(bool visible) override;
	void resize(QSize size) override;
	void setGeometry(QRect rect) override;
	bool isWindow() const override;
	void updateIndicatorVisibility() override;

private:
	KDDockWidgets::Core::ClassicIndicatorWindowViewInterface* window();
	const KDDockWidgets::Core::ClassicIndicatorWindowViewInterface* window() const;

	void recreateWindowIfNecessary();

	KDDockWidgets::Core::ClassicIndicatorWindowViewInterface* m_window = nullptr;
	KDDockWidgets::Core::ClassicIndicatorWindowViewInterface* m_fallback_window = nullptr;

	bool m_supports_compositing = true;
	KDDockWidgets::Core::ClassicDropIndicatorOverlay* m_classic_indicators = nullptr;
};

// Our default custom drop indicator implementation. This fits in with PCSX2's
// themes a lot better, but doesn't support windowing systems where compositing
// is disabled (it would show a black screen).
class DockDropIndicatorWindow : public QWidget, public KDDockWidgets::Core::ClassicIndicatorWindowViewInterface
{
	Q_OBJECT

public:
	DockDropIndicatorWindow(
		KDDockWidgets::Core::ClassicDropIndicatorOverlay* classic_indicators);

	void setObjectName(const QString& name) override;
	KDDockWidgets::DropLocation hover(QPoint globalPos) override;
	QPoint posForIndicator(KDDockWidgets::DropLocation loc) const override;
	void updatePositions() override;
	void raise() override;
	void setVisible(bool visible) override;
	void resize(QSize size) override;
	void setGeometry(QRect rect) override;
	bool isWindow() const override;
	void updateIndicatorVisibility() override;

protected:
	void resizeEvent(QResizeEvent* ev) override;

private:
	KDDockWidgets::Core::ClassicDropIndicatorOverlay* m_classic_indicators;
	std::vector<DockDropIndicator*> m_indicators;
};

class DockDropIndicator : public QWidget
{
	Q_OBJECT

public:
	DockDropIndicator(KDDockWidgets::DropLocation loc, QWidget* parent = nullptr);

	KDDockWidgets::DropLocation location;
	bool hovered = false;

protected:
	void paintEvent(QPaintEvent* event) override;
};

// An alternative drop indicator design that can be enabled from the settings
// menu. For this one we don't need to worry about whether compositing is
// supported since it doesn't create its own window.
class DockSegmentedDropIndicatorOverlay : public KDDockWidgets::QtWidgets::SegmentedDropIndicatorOverlay
{
	Q_OBJECT

public:
	DockSegmentedDropIndicatorOverlay(
		KDDockWidgets::Core::SegmentedDropIndicatorOverlay* controller, QWidget* parent = nullptr);

	static std::string s_indicator_style;

protected:
	void paintEvent(QPaintEvent* event) override;

private:
	void drawSegmented();
	void drawMinimalistic();
};
