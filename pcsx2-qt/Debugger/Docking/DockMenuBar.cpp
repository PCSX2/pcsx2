// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DockMenuBar.h"

#include <QtCore/QTimer>
#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QStyleOption>

static const int OUTER_MENU_MARGIN = 2;
static const int INNER_MENU_MARGIN = 4;

DockMenuBar::DockMenuBar(QWidget* original_menu_bar, QWidget* parent)
	: QWidget(parent)
	, m_original_menu_bar(original_menu_bar)
{
	QHBoxLayout* layout = new QHBoxLayout;
	layout->setContentsMargins(0, OUTER_MENU_MARGIN, OUTER_MENU_MARGIN, 0);
	setLayout(layout);

	QWidget* menu_wrapper = new QWidget;
	menu_wrapper->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
	layout->addWidget(menu_wrapper);

	QHBoxLayout* menu_layout = new QHBoxLayout;
	menu_layout->setContentsMargins(0, INNER_MENU_MARGIN, 0, INNER_MENU_MARGIN);
	menu_wrapper->setLayout(menu_layout);

	menu_layout->addWidget(original_menu_bar);

	m_layout_switcher = new QTabBar;
	m_layout_switcher->setContentsMargins(0, 0, 0, 0);
	m_layout_switcher->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	m_layout_switcher->setContextMenuPolicy(Qt::CustomContextMenu);
	m_layout_switcher->setDrawBase(false);
	m_layout_switcher->setExpanding(false);
	m_layout_switcher->setMovable(true);
	layout->addWidget(m_layout_switcher);

	connect(m_layout_switcher, &QTabBar::tabMoved, this, [this](int from, int to) {
		DockLayout::Index from_index = static_cast<DockLayout::Index>(from);
		DockLayout::Index to_index = static_cast<DockLayout::Index>(to);
		emit layoutMoved(from_index, to_index);
	});

	connect(m_layout_switcher, &QTabBar::customContextMenuRequested, this, [this](const QPoint& pos) {
		emit layoutSwitcherContextMenuRequested(pos, m_layout_switcher);
	});

	m_blink_timer = new QTimer(this);
	connect(m_blink_timer, &QTimer::timeout, this, &DockMenuBar::updateBlink);

	m_layout_locked_toggle = new QPushButton;
	m_layout_locked_toggle->setCheckable(true);
	connect(m_layout_locked_toggle, &QPushButton::clicked, this, [this](bool checked) {
		if (m_ignore_lock_state_changed)
			return;

		emit lockButtonToggled(checked);
	});
	layout->addWidget(m_layout_locked_toggle);

	updateTheme();
}

void DockMenuBar::updateTheme()
{
	DockMenuBarStyle* style = new DockMenuBarStyle(m_layout_switcher);
	m_original_menu_bar->setStyle(style);
	m_layout_switcher->setStyle(style);
	m_layout_locked_toggle->setStyle(style);

	delete m_style;
	m_style = style;
}

void DockMenuBar::updateLayoutSwitcher(DockLayout::Index current_index, const std::vector<DockLayout>& layouts)
{
	disconnect(m_tab_connection);

	for (int i = m_layout_switcher->count(); i > 0; i--)
		m_layout_switcher->removeTab(i - 1);

	for (const DockLayout& layout : layouts)
	{
		const char* cpu_name = DebugInterface::cpuName(layout.cpu());
		QString tab_name = QString("%1 (%2)").arg(layout.name()).arg(cpu_name);
		m_layout_switcher->addTab(tab_name);
	}

	m_plus_tab_index = m_layout_switcher->addTab("+");
	m_current_tab_index = current_index;

	if (current_index != DockLayout::INVALID_INDEX)
		m_layout_switcher->setCurrentIndex(current_index);
	else
		m_layout_switcher->setCurrentIndex(m_plus_tab_index);

	// If we don't have any layouts, the currently selected tab will never be
	// changed, so we respond to all clicks instead.
	if (m_plus_tab_index > 0)
		m_tab_connection = connect(m_layout_switcher, &QTabBar::currentChanged, this, &DockMenuBar::tabChanged);
	else
		m_tab_connection = connect(m_layout_switcher, &QTabBar::tabBarClicked, this, &DockMenuBar::tabChanged);

	stopBlink();
}

void DockMenuBar::onCurrentLayoutChanged(DockLayout::Index current_index)
{
	m_ignore_current_tab_changed = true;

	if (current_index != DockLayout::INVALID_INDEX)
		m_layout_switcher->setCurrentIndex(current_index);
	else
		m_layout_switcher->setCurrentIndex(m_plus_tab_index);

	m_ignore_current_tab_changed = false;
}

void DockMenuBar::onLockStateChanged(bool layout_locked)
{
	m_ignore_lock_state_changed = true;

	m_layout_locked_toggle->setChecked(layout_locked);

	if (layout_locked)
	{
		m_layout_locked_toggle->setText(tr("Layout Locked"));
		m_layout_locked_toggle->setIcon(QIcon::fromTheme(QString::fromUtf8("padlock-lock")));
	}
	else
	{
		m_layout_locked_toggle->setText(tr("Layout Unlocked"));
		m_layout_locked_toggle->setIcon(QIcon::fromTheme(QString::fromUtf8("padlock-unlock")));
	}

	m_ignore_lock_state_changed = false;
}

void DockMenuBar::startBlink(DockLayout::Index layout_index)
{
	stopBlink();

	if (layout_index == DockLayout::INVALID_INDEX)
		return;

	m_blink_tab = static_cast<int>(layout_index);
	m_blink_stage = 0;
	m_blink_timer->start(500);

	updateBlink();
}

void DockMenuBar::updateBlink()
{
	if (m_blink_tab < m_layout_switcher->count())
	{
		if (m_blink_stage % 2 == 0)
			m_layout_switcher->setTabTextColor(m_blink_tab, Qt::red);
		else
			m_layout_switcher->setTabTextColor(m_blink_tab, m_layout_switcher->palette().text().color());
	}

	m_blink_stage++;

	if (m_blink_stage > 7)
		m_blink_timer->stop();
}

void DockMenuBar::stopBlink()
{
	if (m_blink_timer->isActive())
	{
		if (m_blink_tab < m_layout_switcher->count())
			m_layout_switcher->setTabTextColor(m_blink_tab, m_layout_switcher->palette().text().color());

		m_blink_timer->stop();
	}
}

int DockMenuBar::innerHeight() const
{
	return m_original_menu_bar->sizeHint().height() + INNER_MENU_MARGIN * 2;
}

void DockMenuBar::paintEvent(QPaintEvent* event)
{
	QPainter painter(this);

	// This fixes the background colour of the menu bar when using the Windows
	// Vista style.
	QStyleOptionMenuItem menu_option;
	menu_option.palette = palette();
	menu_option.state = QStyle::State_None;
	menu_option.menuItemType = QStyleOptionMenuItem::EmptyArea;
	menu_option.checkType = QStyleOptionMenuItem::NotCheckable;
	menu_option.rect = rect();
	menu_option.menuRect = rect();
	style()->drawControl(QStyle::CE_MenuBarEmptyArea, &menu_option, &painter, this);
}

void DockMenuBar::tabChanged(int index)
{
	// Prevent recursion.
	if (m_ignore_current_tab_changed)
		return;

	if (index < m_plus_tab_index)
	{
		DockLayout::Index layout_index = static_cast<DockLayout::Index>(index);
		emit currentLayoutChanged(layout_index);
	}
	else if (index == m_plus_tab_index)
	{
		emit newButtonClicked();
	}
}

// *****************************************************************************

DockMenuBarStyle::DockMenuBarStyle(QObject* parent)
	: QProxyStyle(QStyleFactory::create(qApp->style()->name()))
{
	setParent(parent);
}

void DockMenuBarStyle::drawControl(
	ControlElement element,
	const QStyleOption* option,
	QPainter* painter,
	const QWidget* widget) const
{
	switch (element)
	{
		case CE_MenuBarItem:
		{
			const QStyleOptionMenuItem* opt = qstyleoption_cast<const QStyleOptionMenuItem*>(option);
			if (!opt)
				break;

			QWidget* menu_wrapper = widget->parentWidget();
			if (!menu_wrapper)
				break;

			const DockMenuBar* menu_bar = qobject_cast<const DockMenuBar*>(menu_wrapper->parentWidget());
			if (!menu_bar)
				break;

			if (baseStyle()->name() != "fusion")
				break;

			// This mirrors a check in QFusionStyle::drawControl. If act is
			// false, QFusionStyle will try to draw a border along the bottom.
			bool act = opt->state & State_Selected && opt->state & State_Sunken;
			if (act)
				break;

			// Extend the menu item to the bottom of the menu bar to fix the
			// position in which it draws its bottom border. We also need to
			// extend it up by the same amount so that the text isn't moved.
			QStyleOptionMenuItem menu_opt = *opt;
			int difference = (menu_bar->innerHeight() - option->rect.top()) - menu_opt.rect.height();
			menu_opt.rect.adjust(0, -difference, 0, difference);
			QProxyStyle::drawControl(element, &menu_opt, painter, widget);

			return;
		}
		case CE_TabBarTab:
		{
			QProxyStyle::drawControl(element, option, painter, widget);

			// Draw a slick-looking highlight under the currently selected tab.
			if (baseStyle()->name() == "fusion")
			{
				const QStyleOptionTab* tab = qstyleoption_cast<const QStyleOptionTab*>(option);
				if (tab && (tab->state & State_Selected))
				{
					painter->setPen(tab->palette.highlight().color());
					painter->drawLine(tab->rect.bottomLeft(), tab->rect.bottomRight());
				}
			}

			return;
		}
		case CE_MenuBarEmptyArea:
		{
			// Prevent it from drawing a border in the wrong position.
			return;
		}
		default:
		{
			break;
		}
	}

	QProxyStyle::drawControl(element, option, painter, widget);
}

QSize DockMenuBarStyle::sizeFromContents(
	QStyle::ContentsType type, const QStyleOption* option, const QSize& contents_size, const QWidget* widget) const
{
	QSize size = QProxyStyle::sizeFromContents(type, option, contents_size, widget);

#ifdef Q_OS_WIN32
	// Adjust the sizes of the layout switcher tabs depending on the theme.
	if (type == CT_TabBarTab)
	{
		const QStyleOptionTab* opt = qstyleoption_cast<const QStyleOptionTab*>(option);
		if (!opt)
			return size;

		const QTabBar* tab_bar = qobject_cast<const QTabBar*>(widget);
		if (!tab_bar)
			return size;

		const DockMenuBar* menu_bar = qobject_cast<const DockMenuBar*>(tab_bar->parentWidget());
		if (!menu_bar)
			return size;

		if (baseStyle()->name() == "fusion" || baseStyle()->name() == "windowsvista")
		{
			// Make sure the tab extends to the bottom of the widget.
			size.setHeight(menu_bar->innerHeight() - opt->rect.top());
		}
		else if (baseStyle()->name() == "windows11")
		{
			// Adjust the size of the tab such that it is vertically centred.
			size.setHeight(menu_bar->innerHeight() - opt->rect.top() * 2 - OUTER_MENU_MARGIN);

			// Make the plus button square.
			if (opt->tabIndex + 1 == tab_bar->count())
				size.setWidth(size.height());
		}
	}
#endif

	return size;
}
