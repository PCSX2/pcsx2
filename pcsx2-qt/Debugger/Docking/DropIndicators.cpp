// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DropIndicators.h"

#include "QtUtils.h"
#include "Debugger/Docking/DockViews.h"

#include "common/Assertions.h"

#include <kddockwidgets/Config.h>
#include <kddockwidgets/core/Group.h>
#include <kddockwidgets/core/indicators/SegmentedDropIndicatorOverlay.h>
#include <kddockwidgets/qtwidgets/ViewFactory.h>

#include <QtGui/QPainter>

static std::pair<QColor, QColor> pickNiceColours(const QPalette& palette, bool hovered)
{
	QColor fill = palette.highlight().color();
	QColor outline = palette.highlight().color();

	if (QtUtils::IsLightTheme(palette))
	{
		fill = fill.darker(200);
		outline = outline.darker(200);
	}
	else
	{
		fill = fill.lighter(200);
		outline = outline.lighter(200);
	}

	fill.setAlpha(200);
	outline.setAlpha(255);

	if (!hovered)
	{
		fill.setAlpha(fill.alpha() / 2);
		outline.setAlpha(outline.alpha() / 2);
	}

	return {fill, outline};
}

// *****************************************************************************

DockDropIndicatorProxy::DockDropIndicatorProxy(KDDockWidgets::Core::ClassicDropIndicatorOverlay* classic_indicators)
	: m_classic_indicators(classic_indicators)
{
	recreateWindowIfNecessary();
}

DockDropIndicatorProxy::~DockDropIndicatorProxy()
{
	delete m_window;
	delete m_fallback_window;
}

void DockDropIndicatorProxy::setObjectName(const QString& name)
{
	window()->setObjectName(name);
}

KDDockWidgets::DropLocation DockDropIndicatorProxy::hover(QPoint globalPos)
{
	return window()->hover(globalPos);
}

QPoint DockDropIndicatorProxy::posForIndicator(KDDockWidgets::DropLocation loc) const
{
	return window()->posForIndicator(loc);
}

void DockDropIndicatorProxy::updatePositions()
{
	// Check if a compositor is running whenever a drag starts.
	recreateWindowIfNecessary();

	window()->updatePositions();
}

void DockDropIndicatorProxy::raise()
{
	window()->raise();
}

void DockDropIndicatorProxy::setVisible(bool visible)
{
	window()->setVisible(visible);
}

void DockDropIndicatorProxy::resize(QSize size)
{
	window()->resize(size);
}

void DockDropIndicatorProxy::setGeometry(QRect rect)
{
	window()->setGeometry(rect);
}

bool DockDropIndicatorProxy::isWindow() const
{
	return window()->isWindow();
}

void DockDropIndicatorProxy::updateIndicatorVisibility()
{
	window()->updateIndicatorVisibility();
}

KDDockWidgets::Core::ClassicIndicatorWindowViewInterface* DockDropIndicatorProxy::window()
{
	if (!m_supports_compositing)
	{
		pxAssert(m_fallback_window);
		return m_fallback_window;
	}

	pxAssert(m_window);
	return m_window;
}

const KDDockWidgets::Core::ClassicIndicatorWindowViewInterface* DockDropIndicatorProxy::window() const
{
	if (!m_supports_compositing)
	{
		pxAssert(m_fallback_window);
		return m_fallback_window;
	}

	pxAssert(m_window);
	return m_window;
}

void DockDropIndicatorProxy::recreateWindowIfNecessary()
{
	bool supports_compositing = QtUtils::IsCompositorManagerRunning();
	if (supports_compositing == m_supports_compositing && (m_window || m_fallback_window))
		return;

	m_supports_compositing = supports_compositing;

	DockViewFactory* factory = static_cast<DockViewFactory*>(KDDockWidgets::Config::self().viewFactory());

	if (supports_compositing)
	{
		if (!m_window)
			m_window = new DockDropIndicatorWindow(m_classic_indicators);

		QWidget* old_window = dynamic_cast<QWidget*>(m_fallback_window);
		if (old_window)
		{
			m_window->setObjectName(old_window->objectName());
			m_window->setVisible(old_window->isVisible());
			m_window->setGeometry(old_window->geometry());
		}

		delete m_fallback_window;
		m_fallback_window = nullptr;
	}
	else
	{
		if (!m_fallback_window)
			m_fallback_window = factory->createFallbackClassicIndicatorWindow(m_classic_indicators, nullptr);

		QWidget* old_window = dynamic_cast<QWidget*>(m_window);
		if (old_window)
		{
			m_window->setObjectName(old_window->objectName());
			m_window->setVisible(old_window->isVisible());
			m_window->setGeometry(old_window->geometry());
		}

		delete m_window;
		m_window = nullptr;
	}
}

// *****************************************************************************

static const constexpr int IND_LEFT = 0;
static const constexpr int IND_TOP = 1;
static const constexpr int IND_RIGHT = 2;
static const constexpr int IND_BOTTOM = 3;
static const constexpr int IND_CENTER = 4;
static const constexpr int IND_OUTER_LEFT = 5;
static const constexpr int IND_OUTER_TOP = 6;
static const constexpr int IND_OUTER_RIGHT = 7;
static const constexpr int IND_OUTER_BOTTOM = 8;

static const constexpr int INDICATOR_SIZE = 40;
static const constexpr int INDICATOR_MARGIN = 10;

static bool isWayland()
{
	return KDDockWidgets::Core::Platform::instance()->displayType() ==
		   KDDockWidgets::Core::Platform::DisplayType::Wayland;
}

static QWidget* parentForIndicatorWindow(KDDockWidgets::Core::ClassicDropIndicatorOverlay* classic_indicators)
{
	if (isWayland())
		return KDDockWidgets::QtCommon::View_qt::asQWidget(classic_indicators->view());

	return nullptr;
}

static Qt::WindowFlags flagsForIndicatorWindow()
{
	if (isWayland())
		return Qt::Widget;

	return Qt::Tool | Qt::BypassWindowManagerHint;
}

DockDropIndicatorWindow::DockDropIndicatorWindow(
	KDDockWidgets::Core::ClassicDropIndicatorOverlay* classic_indicators)
	: QWidget(parentForIndicatorWindow(classic_indicators), flagsForIndicatorWindow())
	, m_classic_indicators(classic_indicators)
	, m_indicators({
		  /* [IND_LEFT] = */ new DockDropIndicator(KDDockWidgets::DropLocation_Left, this),
		  /* [IND_TOP] = */ new DockDropIndicator(KDDockWidgets::DropLocation_Top, this),
		  /* [IND_RIGHT] = */ new DockDropIndicator(KDDockWidgets::DropLocation_Right, this),
		  /* [IND_BOTTOM] = */ new DockDropIndicator(KDDockWidgets::DropLocation_Bottom, this),
		  /* [IND_CENTER] = */ new DockDropIndicator(KDDockWidgets::DropLocation_Center, this),
		  /* [IND_OUTER_LEFT] = */ new DockDropIndicator(KDDockWidgets::DropLocation_OutterLeft, this),
		  /* [IND_OUTER_TOP] = */ new DockDropIndicator(KDDockWidgets::DropLocation_OutterTop, this),
		  /* [IND_OUTER_RIGHT] = */ new DockDropIndicator(KDDockWidgets::DropLocation_OutterRight, this),
		  /* [IND_OUTER_BOTTOM] = */ new DockDropIndicator(KDDockWidgets::DropLocation_OutterBottom, this),
	  })
{
	setWindowFlag(Qt::FramelessWindowHint, true);

	if (KDDockWidgets::Config::self().flags() & KDDockWidgets::Config::Flag_KeepAboveIfNotUtilityWindow)
		setWindowFlag(Qt::WindowStaysOnTopHint, true);

	setAttribute(Qt::WA_TranslucentBackground);
}

void DockDropIndicatorWindow::setObjectName(const QString& name)
{
	QWidget::setObjectName(name);
}

KDDockWidgets::DropLocation DockDropIndicatorWindow::hover(QPoint globalPos)
{
	KDDockWidgets::DropLocation result = KDDockWidgets::DropLocation_None;

	for (DockDropIndicator* indicator : m_indicators)
	{
		if (indicator->isVisible())
		{
			bool hovered = indicator->rect().contains(indicator->mapFromGlobal(globalPos));
			if (hovered != indicator->hovered)
			{
				indicator->hovered = hovered;
				indicator->update();
			}

			if (hovered)
				result = indicator->location;
		}
	}

	return result;
}

QPoint DockDropIndicatorWindow::posForIndicator(KDDockWidgets::DropLocation loc) const
{
	for (DockDropIndicator* indicator : m_indicators)
		if (indicator->location == loc)
			return indicator->mapToGlobal(indicator->rect().center());

	return QPoint();
}

void DockDropIndicatorWindow::updatePositions()
{
	DockDropIndicator* left = m_indicators[IND_LEFT];
	DockDropIndicator* top = m_indicators[IND_TOP];
	DockDropIndicator* right = m_indicators[IND_RIGHT];
	DockDropIndicator* bottom = m_indicators[IND_BOTTOM];
	DockDropIndicator* center = m_indicators[IND_CENTER];
	DockDropIndicator* outer_left = m_indicators[IND_OUTER_LEFT];
	DockDropIndicator* outer_top = m_indicators[IND_OUTER_TOP];
	DockDropIndicator* outer_right = m_indicators[IND_OUTER_RIGHT];
	DockDropIndicator* outer_bottom = m_indicators[IND_OUTER_BOTTOM];

	QRect r = rect();
	int half_indicator_width = INDICATOR_SIZE / 2;

	outer_left->move(r.x() + INDICATOR_MARGIN, r.center().y() - half_indicator_width);
	outer_bottom->move(r.center().x() - half_indicator_width, r.y() + height() - INDICATOR_SIZE - INDICATOR_MARGIN);
	outer_top->move(r.center().x() - half_indicator_width, r.y() + INDICATOR_MARGIN);
	outer_right->move(r.x() + width() - INDICATOR_SIZE - INDICATOR_MARGIN, r.center().y() - half_indicator_width);

	KDDockWidgets::Core::Group* hovered_group = m_classic_indicators->hoveredGroup();
	if (hovered_group)
	{
		QRect hoveredRect = hovered_group->view()->geometry();
		center->move(r.topLeft() + hoveredRect.center() - QPoint(half_indicator_width, half_indicator_width));
		top->move(center->pos() - QPoint(0, INDICATOR_SIZE + INDICATOR_MARGIN));
		right->move(center->pos() + QPoint(INDICATOR_SIZE + INDICATOR_MARGIN, 0));
		bottom->move(center->pos() + QPoint(0, INDICATOR_SIZE + INDICATOR_MARGIN));
		left->move(center->pos() - QPoint(INDICATOR_SIZE + INDICATOR_MARGIN, 0));
	}
}

void DockDropIndicatorWindow::raise()
{
	QWidget::raise();
}

void DockDropIndicatorWindow::setVisible(bool is)
{
	QWidget::setVisible(is);
}

void DockDropIndicatorWindow::resize(QSize size)
{
	QWidget::resize(size);
}

void DockDropIndicatorWindow::setGeometry(QRect rect)
{
	QWidget::setGeometry(rect);
}

bool DockDropIndicatorWindow::isWindow() const
{
	return QWidget::isWindow();
}

void DockDropIndicatorWindow::updateIndicatorVisibility()
{
	for (DockDropIndicator* indicator : m_indicators)
		indicator->setVisible(m_classic_indicators->dropIndicatorVisible(indicator->location));
}

void DockDropIndicatorWindow::resizeEvent(QResizeEvent* ev)
{
	QWidget::resizeEvent(ev);
	updatePositions();
}

// *****************************************************************************

DockDropIndicator::DockDropIndicator(KDDockWidgets::DropLocation loc, QWidget* parent)
	: QWidget(parent)
	, location(loc)
{
	setFixedSize(INDICATOR_SIZE, INDICATOR_SIZE);
	setVisible(true);
}

void DockDropIndicator::paintEvent(QPaintEvent* event)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing, true);

	auto [fill, outline] = pickNiceColours(palette(), hovered);

	painter.setBrush(fill);

	QPen pen;
	pen.setColor(outline);
	pen.setWidth(2);
	painter.setPen(pen);

	painter.drawRect(rect());

	QRectF rf = rect().toRectF();

	QRectF outer = rf.marginsRemoved(QMarginsF(4.f, 4.f, 4.f, 4.f));
	QPointF icon_position;
	switch (location)
	{
		case KDDockWidgets::DropLocation_Left:
		case KDDockWidgets::DropLocation_OutterLeft:
			outer = outer.marginsRemoved(QMarginsF(0.f, 0.f, outer.width() / 2.f, 0.f));
			icon_position = rf.marginsRemoved(QMarginsF(rf.width() / 2.f, 0.f, 0.f, 0.f)).center();
			break;
		case KDDockWidgets::DropLocation_Top:
		case KDDockWidgets::DropLocation_OutterTop:
			outer = outer.marginsRemoved(QMarginsF(0.f, 0.f, 0.f, outer.width() / 2.f));
			icon_position = rf.marginsRemoved(QMarginsF(0.f, rf.width() / 2.f, 0.f, 0.f)).center();
			break;
		case KDDockWidgets::DropLocation_Right:
		case KDDockWidgets::DropLocation_OutterRight:
			outer = outer.marginsRemoved(QMarginsF(outer.width() / 2.f, 0.f, 0.f, 0.f));
			icon_position = rf.marginsRemoved(QMarginsF(0.f, 0.f, rf.width() / 2.f, 0.f)).center();
			break;
		case KDDockWidgets::DropLocation_Bottom:
		case KDDockWidgets::DropLocation_OutterBottom:
			outer = outer.marginsRemoved(QMarginsF(0.f, outer.width() / 2.f, 0.f, 0.f));
			icon_position = rf.marginsRemoved(QMarginsF(0.f, 0.f, 0.f, rf.width() / 2.f)).center();
			break;
		default:
		{
		}
	}

	painter.drawRect(outer);

	float arrow_size = INDICATOR_SIZE / 10.f;

	QPolygonF arrow;
	switch (location)
	{
		case KDDockWidgets::DropLocation_Left:
			arrow = {
				icon_position + QPointF(-arrow_size, 0.f),
				icon_position + QPointF(arrow_size, arrow_size * 2.f),
				icon_position + QPointF(arrow_size, -arrow_size * 2.f),
			};
			break;
		case KDDockWidgets::DropLocation_Top:
			arrow = {
				icon_position + QPointF(0.f, -arrow_size),
				icon_position + QPointF(arrow_size * 2.f, arrow_size),
				icon_position + QPointF(-arrow_size * 2.f, arrow_size),
			};
			break;
		case KDDockWidgets::DropLocation_Right:
			arrow = {
				icon_position + QPointF(arrow_size, 0.f),
				icon_position + QPointF(-arrow_size, arrow_size * 2.f),
				icon_position + QPointF(-arrow_size, -arrow_size * 2.f),
			};
			break;
		case KDDockWidgets::DropLocation_Bottom:
			arrow = {
				icon_position + QPointF(0.f, arrow_size),
				icon_position + QPointF(arrow_size * 2.f, -arrow_size),
				icon_position + QPointF(-arrow_size * 2.f, -arrow_size),
			};
			break;
		default:
		{
		}
	}

	painter.drawPolygon(arrow);
}

// *****************************************************************************

std::string DockSegmentedDropIndicatorOverlay::s_indicator_style;

DockSegmentedDropIndicatorOverlay::DockSegmentedDropIndicatorOverlay(
	KDDockWidgets::Core::SegmentedDropIndicatorOverlay* controller, QWidget* parent)
	: KDDockWidgets::QtWidgets::SegmentedDropIndicatorOverlay(controller, parent)
{
}

void DockSegmentedDropIndicatorOverlay::paintEvent(QPaintEvent* event)
{
	if (s_indicator_style == "Minimalistic")
		drawMinimalistic();
	else
		drawSegmented();
}

void DockSegmentedDropIndicatorOverlay::drawSegmented()
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing, true);

	KDDockWidgets::Core::SegmentedDropIndicatorOverlay* controller =
		asController<KDDockWidgets::Core::SegmentedDropIndicatorOverlay>();

	const std::unordered_map<KDDockWidgets::DropLocation, QPolygon>& segments = controller->segments();

	for (KDDockWidgets::DropLocation location :
		{KDDockWidgets::DropLocation_Left,
			KDDockWidgets::DropLocation_Top,
			KDDockWidgets::DropLocation_Right,
			KDDockWidgets::DropLocation_Bottom,
			KDDockWidgets::DropLocation_Center,
			KDDockWidgets::DropLocation_OutterLeft,
			KDDockWidgets::DropLocation_OutterTop,
			KDDockWidgets::DropLocation_OutterRight,
			KDDockWidgets::DropLocation_OutterBottom})
	{
		auto segment = segments.find(location);
		if (segment == segments.end() || segment->second.size() < 2)
			continue;

		bool hovered = segment->second.containsPoint(controller->hoveredPt(), Qt::OddEvenFill);
		auto [fill, outline] = pickNiceColours(palette(), hovered);

		painter.setBrush(fill);

		QPen pen(outline);
		pen.setWidth(1);
		painter.setPen(pen);

		int margin = KDDockWidgets::Core::SegmentedDropIndicatorOverlay::s_segmentGirth * 2;

		// Make sure the rectangles don't intersect with each other.
		QRect rect;
		switch (location)
		{
			case KDDockWidgets::DropLocation_Top:
			case KDDockWidgets::DropLocation_Bottom:
			case KDDockWidgets::DropLocation_OutterTop:
			case KDDockWidgets::DropLocation_OutterBottom:
			{
				rect = segment->second.boundingRect().marginsRemoved(QMargins(margin, 4, margin, 4));
				break;
			}
			case KDDockWidgets::DropLocation_Left:
			case KDDockWidgets::DropLocation_Right:
			case KDDockWidgets::DropLocation_OutterLeft:
			case KDDockWidgets::DropLocation_OutterRight:
			{
				rect = segment->second.boundingRect().marginsRemoved(QMargins(4, margin, 4, margin));
				break;
			}
			default:
			{
				rect = segment->second.boundingRect().marginsRemoved(QMargins(4, 4, 4, 4));
				break;
			}
		}

		painter.drawRect(rect);
	}
}

void DockSegmentedDropIndicatorOverlay::drawMinimalistic()
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing, true);

	KDDockWidgets::Core::SegmentedDropIndicatorOverlay* controller =
		asController<KDDockWidgets::Core::SegmentedDropIndicatorOverlay>();

	const std::unordered_map<KDDockWidgets::DropLocation, QPolygon>& segments = controller->segments();

	for (KDDockWidgets::DropLocation location :
		{KDDockWidgets::DropLocation_Left,
			KDDockWidgets::DropLocation_Top,
			KDDockWidgets::DropLocation_Right,
			KDDockWidgets::DropLocation_Bottom,
			KDDockWidgets::DropLocation_Center,
			KDDockWidgets::DropLocation_OutterLeft,
			KDDockWidgets::DropLocation_OutterTop,
			KDDockWidgets::DropLocation_OutterRight,
			KDDockWidgets::DropLocation_OutterBottom})
	{
		auto segment = segments.find(location);
		if (segment == segments.end() || segment->second.size() < 2)
			continue;

		if (!segment->second.containsPoint(controller->hoveredPt(), Qt::OddEvenFill))
			continue;

		auto [fill, outline] = pickNiceColours(palette(), true);

		painter.setBrush(fill);

		QPen pen(outline);
		pen.setWidth(1);
		painter.setPen(pen);

		painter.drawRect(segment->second.boundingRect());
	}
}
