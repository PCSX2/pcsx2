// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "RegisterView.h"

#include "AsyncDialogs.h"
#include "QtUtils.h"
#include "Debugger/JsonValueWrapper.h"

#include <QtGui/QMouseEvent>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QStylePainter>
#include <QtWidgets/QStyleOptionTab>
#include <QtGui/QClipboard>
#include <QtWidgets/QProxyStyle>

#include <bit>

#define CAT_SHOW_FLOAT (categoryIndex == EECAT_FPR && m_showFPRFloat) || (categoryIndex == EECAT_VU0F && m_showVU0FFloat)

using namespace QtUtils;

RegisterView::RegisterView(const DebuggerViewParameters& parameters)
	: DebuggerView(parameters, MONOSPACE_FONT)
{
	this->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);

	ui.setupUi(this);
	ui.registerTabs->setDrawBase(false);

	connect(this, &RegisterView::customContextMenuRequested, this, &RegisterView::customMenuRequested);
	connect(ui.registerTabs, &QTabBar::currentChanged, this, &RegisterView::tabCurrentChanged);

	for (int i = 0; i < cpu().getRegisterCategoryCount(); i++)
	{
		ui.registerTabs->addTab(cpu().getRegisterCategoryName(i));
	}

	connect(ui.registerTabs, &QTabBar::currentChanged, [this]() { this->repaint(); });

	receiveEvent<DebuggerEvents::Refresh>([this](const DebuggerEvents::Refresh& event) -> bool {
		update();
		return true;
	});
}

RegisterView::~RegisterView()
{
}

void RegisterView::toJson(JsonValueWrapper& json)
{
	DebuggerView::toJson(json);

	json.value().AddMember("showVU0FFloat", m_showVU0FFloat, json.allocator());
	json.value().AddMember("showFPRFloat", m_showFPRFloat, json.allocator());
}

bool RegisterView::fromJson(const JsonValueWrapper& json)
{
	if (!DebuggerView::fromJson(json))
		return false;

	auto show_vu0f_float = json.value().FindMember("showVU0FFloat");
	if (show_vu0f_float != json.value().MemberEnd() && show_vu0f_float->value.IsBool())
		m_showVU0FFloat = show_vu0f_float->value.GetBool();

	auto show_fpr_float = json.value().FindMember("showFPRFloat");
	if (show_fpr_float != json.value().MemberEnd() && show_fpr_float->value.IsBool())
		m_showFPRFloat = show_fpr_float->value.GetBool();

	repaint();

	return true;
}

void RegisterView::tabCurrentChanged(int cur)
{
	m_rowStart = 0;
}

void RegisterView::paintEvent(QPaintEvent* event)
{
	QPainter painter(this);
	painter.setPen(this->palette().text().color());
	m_renderStart = QPoint(0, ui.registerTabs->pos().y() + ui.registerTabs->size().height());
	const QSize renderSize = QSize(this->size().width(), this->size().height() - ui.registerTabs->size().height());

	m_rowHeight = painter.fontMetrics().height() + 2;
	m_rowEnd = m_rowStart + (renderSize.height() / m_rowHeight) - 1; // Maybe move this to a onsize event

	bool alternate = m_rowStart % 2;

	const int categoryIndex = ui.registerTabs->currentIndex();

	// Used for 128 bit and VU0f registers
	const int titleStartX = m_renderStart.x() + (painter.fontMetrics().averageCharWidth() * 6);
	m_fieldWidth = ((renderSize.width() - (painter.fontMetrics().averageCharWidth() * 6)) / 4);

	m_fieldStartX[0] = titleStartX;
	m_fieldStartX[1] = titleStartX + m_fieldWidth;
	m_fieldStartX[2] = titleStartX + (m_fieldWidth * 2);
	m_fieldStartX[3] = titleStartX + (m_fieldWidth * 3);

	if (categoryIndex == EECAT_VU0F)
	{
		painter.fillRect(m_renderStart.x(), m_renderStart.y(), renderSize.width(), m_rowHeight, this->palette().highlight());

		painter.drawText(m_fieldStartX[0], m_renderStart.y(), m_fieldWidth, m_rowHeight, Qt::AlignLeft, "W");
		painter.drawText(m_fieldStartX[1], m_renderStart.y(), m_fieldWidth, m_rowHeight, Qt::AlignLeft, "Z");
		painter.drawText(m_fieldStartX[2], m_renderStart.y(), m_fieldWidth, m_rowHeight, Qt::AlignLeft, "Y");
		painter.drawText(m_fieldStartX[3], m_renderStart.y(), m_fieldWidth, m_rowHeight, Qt::AlignLeft, "X");

		m_renderStart += QPoint(0, m_rowHeight); // Make room for VU0f titles
	}

	// Find the longest register name and calculate where to place our values
	// off of that.
	// Can probably constexpr the loop out as register names are known during runtime
	int safeValueStartX = 0;
	for (int i = 0; i < cpu().getRegisterCount(categoryIndex); i++)
	{
		const int registerNameWidth = strlen(cpu().getRegisterName(categoryIndex, i));
		if (safeValueStartX < registerNameWidth)
		{
			safeValueStartX = registerNameWidth;
		}
	}

	// Add a space between the value and name
	safeValueStartX += 2;
	// Convert to width in pixels
	safeValueStartX *= painter.fontMetrics().averageCharWidth();
	// Make it relative to where we start rendering
	safeValueStartX += m_renderStart.x();

	for (s32 i = 0; i < cpu().getRegisterCount(categoryIndex) - m_rowStart; i++)
	{
		const s32 registerIndex = i + m_rowStart;
		const int yStart = (i * m_rowHeight) + m_renderStart.y();

		painter.fillRect(m_renderStart.x(), yStart, renderSize.width(), m_rowHeight, alternate ? this->palette().base() : this->palette().alternateBase());
		alternate = !alternate;

		// Draw register name
		painter.setPen(this->palette().text().color());
		painter.drawText(m_renderStart.x() + painter.fontMetrics().averageCharWidth(), yStart, renderSize.width(), m_rowHeight, Qt::AlignLeft, cpu().getRegisterName(categoryIndex, registerIndex));

		if (cpu().getRegisterSize(categoryIndex) == 128)
		{
			const u128 curRegister = cpu().getRegister(categoryIndex, registerIndex);

			int regIndex = 3;
			for (int j = 0; j < 4; j++)
			{
				if (m_selectedRow == registerIndex && m_selected128Field == j)
					painter.setPen(this->palette().highlight().color());
				else
					painter.setPen(this->palette().text().color());

				if (categoryIndex == EECAT_VU0F && m_showVU0FFloat)
					painter.drawText(m_fieldStartX[j], yStart, m_fieldWidth, m_rowHeight, Qt::AlignLeft,
						painter.fontMetrics().elidedText(QString::number(std::bit_cast<float>(cpu().getRegister(categoryIndex, registerIndex)._u32[regIndex])), Qt::ElideRight, m_fieldWidth - painter.fontMetrics().averageCharWidth()));
				else
					painter.drawText(m_fieldStartX[j], yStart, m_fieldWidth, m_rowHeight,
						Qt::AlignLeft, FilledQStringFromValue(curRegister._u32[regIndex], 16));
				regIndex--;
			}
			painter.setPen(this->palette().text().color());
		}
		else
		{
			if (m_selectedRow == registerIndex)
				painter.setPen(this->palette().highlight().color());
			else
				painter.setPen(this->palette().text().color());

			if (categoryIndex == EECAT_FPR && m_showFPRFloat)
				painter.drawText(safeValueStartX, yStart, renderSize.width(), m_rowHeight, Qt::AlignLeft,
					QString("%1").arg(QString::number(std::bit_cast<float>(cpu().getRegister(categoryIndex, registerIndex)._u32[0]))).toUpper());
			else if (cpu().getRegisterSize(categoryIndex) == 64)
				painter.drawText(safeValueStartX, yStart, renderSize.width(), m_rowHeight, Qt::AlignLeft,
					FilledQStringFromValue(cpu().getRegister(categoryIndex, registerIndex).lo, 16));
			else
				painter.drawText(safeValueStartX, yStart, renderSize.width(), m_rowHeight, Qt::AlignLeft,
					FilledQStringFromValue(cpu().getRegister(categoryIndex, registerIndex)._u32[0], 16));
		}
	}
	painter.end();
}

void RegisterView::mousePressEvent(QMouseEvent* event)
{
	const int categoryIndex = ui.registerTabs->currentIndex();
	m_selectedRow = static_cast<int>(((event->position().y() - m_renderStart.y()) / m_rowHeight)) + m_rowStart;

	// For 128 bit types, support selecting segments
	if (cpu().getRegisterSize(categoryIndex) == 128)
	{
		constexpr auto inRange = [](u32 low, u32 high, u32 val) {
			return (low <= val && val <= high);
		};

		for (int i = 0; i < 4; i++)
		{
			if (inRange(m_fieldStartX[i], m_fieldStartX[i] + m_fieldWidth, event->position().x()))
			{
				m_selected128Field = i;
			}
		}
	}
	this->repaint();
}

void RegisterView::wheelEvent(QWheelEvent* event)
{
	if (event->angleDelta().y() < 0 && m_rowEnd < cpu().getRegisterCount(ui.registerTabs->currentIndex()))
	{
		m_rowStart += 1;
	}
	else if (event->angleDelta().y() > 0 && m_rowStart > 0)
	{
		m_rowStart -= 1;
	}

	this->repaint();
}

void RegisterView::mouseDoubleClickEvent(QMouseEvent* event)
{
	if (!cpu().isAlive())
		return;
	if (m_selectedRow > m_rowEnd) // Unsigned underflow; selectedRow will be > m_rowEnd (technically negative)
		return;
	const int categoryIndex = ui.registerTabs->currentIndex();
	if (cpu().getRegisterSize(categoryIndex) == 128)
		contextChangeSegment();
	else
		contextChangeValue();
}

void RegisterView::customMenuRequested(QPoint pos)
{
	if (!cpu().isAlive())
		return;

	if (m_selectedRow > m_rowEnd) // Unsigned underflow; selectedRow will be > m_rowEnd (technically negative)
		return;

	QMenu* menu = new QMenu(this);
	menu->setAttribute(Qt::WA_DeleteOnClose);

	const int categoryIndex = ui.registerTabs->currentIndex();

	if (categoryIndex == EECAT_FPR)
	{
		QAction* action = menu->addAction(tr("Show as Float"));
		action->setCheckable(true);
		action->setChecked(m_showFPRFloat);
		connect(action, &QAction::triggered, this, [this]() {
			m_showFPRFloat = !m_showFPRFloat;
			repaint();
		});

		menu->addSeparator();
	}

	if (categoryIndex == EECAT_VU0F)
	{
		QAction* action = menu->addAction(tr("Show as Float"));
		action->setCheckable(true);
		action->setChecked(m_showVU0FFloat);
		connect(action, &QAction::triggered, this, [this]() {
			m_showVU0FFloat = !m_showVU0FFloat;
			repaint();
		});

		menu->addSeparator();
	}

	if (cpu().getRegisterSize(categoryIndex) == 128)
	{
		connect(menu->addAction(tr("Copy Top Half")), &QAction::triggered, this, &RegisterView::contextCopyTop);
		connect(menu->addAction(tr("Copy Bottom Half")), &QAction::triggered, this, &RegisterView::contextCopyBottom);
		connect(menu->addAction(tr("Copy Segment")), &QAction::triggered, this, &RegisterView::contextCopySegment);
	}
	else
	{
		connect(menu->addAction(tr("Copy Value")), &QAction::triggered, this, &RegisterView::contextCopyValue);
	}

	menu->addSeparator();

	if (cpu().getRegisterSize(categoryIndex) == 128)
	{
		connect(menu->addAction(tr("Change Top Half")), &QAction::triggered,
			this, &RegisterView::contextChangeTop);
		connect(menu->addAction(tr("Change Bottom Half")), &QAction::triggered,
			this, &RegisterView::contextChangeBottom);
		connect(menu->addAction(tr("Change Segment")), &QAction::triggered,
			this, &RegisterView::contextChangeSegment);
	}
	else
	{
		connect(menu->addAction(tr("Change Value")), &QAction::triggered,
			this, &RegisterView::contextChangeValue);
	}

	menu->addSeparator();

	createEventActions<DebuggerEvents::GoToAddress>(menu, [this]() {
		return contextCreateGotoEvent();
	});

	menu->popup(this->mapToGlobal(pos));
}


void RegisterView::contextCopyValue()
{
	const int categoryIndex = ui.registerTabs->currentIndex();
	const u128 val = cpu().getRegister(categoryIndex, m_selectedRow);
	if (CAT_SHOW_FLOAT)
		QApplication::clipboard()->setText(QString("%1").arg(QString::number(std::bit_cast<float>(val._u32[0])).toUpper(), 16));
	else
		QApplication::clipboard()->setText(QString("%1").arg(QString::number(val._u64[0], 16).toUpper(), 16));
}

void RegisterView::contextCopyTop()
{
	const int categoryIndex = ui.registerTabs->currentIndex();
	const u128 val = cpu().getRegister(categoryIndex, m_selectedRow);
	QApplication::clipboard()->setText(FilledQStringFromValue(val.hi, 16));
}

void RegisterView::contextCopyBottom()
{
	const int categoryIndex = ui.registerTabs->currentIndex();
	const u128 val = cpu().getRegister(categoryIndex, m_selectedRow);
	QApplication::clipboard()->setText(FilledQStringFromValue(val.lo, 16));
}

void RegisterView::contextCopySegment()
{
	const int categoryIndex = ui.registerTabs->currentIndex();
	const u128 val = cpu().getRegister(categoryIndex, m_selectedRow);
	if (CAT_SHOW_FLOAT)
		QApplication::clipboard()->setText(FilledQStringFromValue(std::bit_cast<float>(val._u32[3 - m_selected128Field]), 10));
	else
		QApplication::clipboard()->setText(FilledQStringFromValue(val._u32[3 - m_selected128Field], 16));
}

void RegisterView::fetchNewValue(u64 currentValue, bool segment, std::function<void(u64)> callback)
{
	const int categoryIndex = ui.registerTabs->currentIndex();
	const bool floatingPoint = CAT_SHOW_FLOAT && segment;
	const int regSize = cpu().getRegisterSize(categoryIndex);

	QString existingValue("%1");

	if (!floatingPoint)
		existingValue = existingValue.arg(currentValue, regSize == 64 ? 16 : 8, 16, QChar('0'));
	else
		existingValue = existingValue.arg(std::bit_cast<float>((u32)currentValue));

	//: Changing the value in a CPU register (e.g. "Change t0")
	const QString title = tr("Change %1").arg(cpu().getRegisterName(categoryIndex, m_selectedRow));

	AsyncDialogs::getText(this, title, "", existingValue, [this, callback, floatingPoint](QString input) {
		u64 value;
		if (!floatingPoint) // Get input as hexadecimal
		{
			bool ok;
			value = input.toULongLong(&ok, 16);
			if (!ok)
			{
				AsyncDialogs::warning(this, tr("Invalid register value"), tr("Invalid hexadecimal register value."));
				return;
			}
		}
		else
		{
			bool ok;
			value = std::bit_cast<u32>(input.toFloat(&ok));
			if (!ok)
			{
				AsyncDialogs::warning(this, tr("Invalid register value"), tr("Invalid floating-point register value."));
				return;
			}
		}

		callback(value);
	});
}

void RegisterView::contextChangeValue()
{
	const int category_index = ui.registerTabs->currentIndex();
	const s32 row = m_selectedRow;
	const u32 old_value = cpu().getRegister(category_index, row).lo;

	fetchNewValue(old_value, false, [this, category_index, row](u64 input) {
		cpu().setRegister(category_index, row, u128::From64(input));
		DebuggerView::broadcastEvent(DebuggerEvents::VMUpdate());
	});
}

void RegisterView::contextChangeTop()
{
	const int category_index = ui.registerTabs->currentIndex();
	const s32 row = m_selectedRow;
	const u128 old_value = cpu().getRegister(category_index, row);

	fetchNewValue(old_value.hi, false, [this, category_index, row, old_value](u32 input) {
		u128 new_value = old_value;
		new_value.hi = input;
		cpu().setRegister(category_index, row, new_value);
		DebuggerView::broadcastEvent(DebuggerEvents::VMUpdate());
	});
}

void RegisterView::contextChangeBottom()
{
	const int category_index = ui.registerTabs->currentIndex();
	const s32 row = m_selectedRow;
	const u128 old_value = cpu().getRegister(category_index, row);

	fetchNewValue(old_value.lo, false, [this, category_index, row, old_value](u32 input) {
		u128 new_value = old_value;
		new_value.lo = input;
		cpu().setRegister(category_index, row, new_value);
		DebuggerView::broadcastEvent(DebuggerEvents::VMUpdate());
	});
}

void RegisterView::contextChangeSegment()
{
	const int category_index = ui.registerTabs->currentIndex();
	const s32 row = m_selectedRow;
	const s32 field = m_selected128Field;
	const u128 old_value = cpu().getRegister(category_index, row);
	const u32 segment = old_value._u32[3 - field];

	fetchNewValue(segment, false, [this, category_index, row, field, old_value](u32 input) {
		u128 new_value = old_value;
		new_value._u32[3 - field] = input;
		cpu().setRegister(category_index, row, new_value);
		DebuggerView::broadcastEvent(DebuggerEvents::VMUpdate());
	});
}

std::optional<DebuggerEvents::GoToAddress> RegisterView::contextCreateGotoEvent()
{
	const int categoryIndex = ui.registerTabs->currentIndex();
	u128 regVal = cpu().getRegister(categoryIndex, m_selectedRow);
	u32 addr = 0;

	if (cpu().getRegisterSize(categoryIndex) == 128)
		addr = regVal._u32[3 - m_selected128Field];
	else
		addr = regVal._u32[0];

	if (!cpu().isValidAddress(addr))
	{
		AsyncDialogs::warning(
			this,
			tr("Invalid target address"),
			tr("This register holds an invalid address."));
		return std::nullopt;
	}

	DebuggerEvents::GoToAddress event;
	event.address = addr;
	return event;
}
