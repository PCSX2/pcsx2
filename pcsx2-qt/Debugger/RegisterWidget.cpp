// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "RegisterWidget.h"

#include "QtUtils.h"
#include <QtGui/QMouseEvent>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QStylePainter>
#include <QtWidgets/QStyleOptionTab>
#include <QtGui/QClipboard>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QProxyStyle>
#include <QtWidgets/QMessageBox>

#include <algorithm>
#include <bit>

#define CAT_SHOW_FLOAT (categoryIndex == EECAT_FPR && m_showFPRFloat) || (categoryIndex == EECAT_VU0F && m_showVU0FFloat)

using namespace QtUtils;

RegisterWidget::RegisterWidget(QWidget* parent)
	: QWidget(parent)
{
	this->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);

	ui.setupUi(this);
	ui.registerTabs->setDrawBase(false);

	connect(this, &RegisterWidget::customContextMenuRequested, this, &RegisterWidget::customMenuRequested);
	connect(ui.registerTabs, &QTabBar::currentChanged, this, &RegisterWidget::tabCurrentChanged);
};

RegisterWidget::~RegisterWidget()
{
}

void RegisterWidget::SetCpu(DebugInterface* cpu)
{
	m_cpu = cpu;
	for (int i = 0; i < m_cpu->getRegisterCategoryCount(); i++)
	{
		ui.registerTabs->addTab(m_cpu->getRegisterCategoryName(i));
	}

	connect(ui.registerTabs, &QTabBar::currentChanged, [this]() { this->repaint(); });
}

void RegisterWidget::tabCurrentChanged(int cur)
{
	m_rowStart = 0;
}

void RegisterWidget::paintEvent(QPaintEvent* event)
{
	if (!m_cpu)
		return;

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
	for (int i = 0; i < m_cpu->getRegisterCount(categoryIndex); i++)
	{
		const int registerNameWidth = strlen(m_cpu->getRegisterName(categoryIndex, i));
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

	for (s32 i = 0; i < m_cpu->getRegisterCount(categoryIndex) - m_rowStart; i++)
	{
		const s32 registerIndex = i + m_rowStart;
		const int yStart = (i * m_rowHeight) + m_renderStart.y();

		painter.fillRect(m_renderStart.x(), yStart, renderSize.width(), m_rowHeight, alternate ? this->palette().base() : this->palette().alternateBase());
		alternate = !alternate;

		// Draw register name
		painter.setPen(this->palette().text().color());
		painter.drawText(m_renderStart.x() + painter.fontMetrics().averageCharWidth(), yStart, renderSize.width(), m_rowHeight, Qt::AlignLeft, m_cpu->getRegisterName(categoryIndex, registerIndex));

		if (m_cpu->getRegisterSize(categoryIndex) == 128)
		{
			const u128 curRegister = m_cpu->getRegister(categoryIndex, registerIndex);

			int regIndex = 3;
			for (int j = 0; j < 4; j++)
			{
				if (m_selectedRow == registerIndex && m_selected128Field == j)
					painter.setPen(this->palette().highlight().color());
				else
					painter.setPen(this->palette().text().color());

				if (categoryIndex == EECAT_VU0F && m_showVU0FFloat)
					painter.drawText(m_fieldStartX[j], yStart, m_fieldWidth, m_rowHeight, Qt::AlignLeft,
						painter.fontMetrics().elidedText(QString::number(std::bit_cast<float>(m_cpu->getRegister(categoryIndex, registerIndex)._u32[regIndex])), Qt::ElideRight, m_fieldWidth - painter.fontMetrics().averageCharWidth()));
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
					QString("%1").arg(QString::number(std::bit_cast<float>(m_cpu->getRegister(categoryIndex, registerIndex)._u32[0]))).toUpper());
			else if (m_cpu->getRegisterSize(categoryIndex) == 64)
				painter.drawText(safeValueStartX, yStart, renderSize.width(), m_rowHeight, Qt::AlignLeft,
					FilledQStringFromValue(m_cpu->getRegister(categoryIndex, registerIndex).lo, 16));
			else
				painter.drawText(safeValueStartX, yStart, renderSize.width(), m_rowHeight, Qt::AlignLeft,
					FilledQStringFromValue(m_cpu->getRegister(categoryIndex, registerIndex)._u32[0], 16));
		}
	}
	painter.end();
}

void RegisterWidget::mousePressEvent(QMouseEvent* event)
{
	const int categoryIndex = ui.registerTabs->currentIndex();
	m_selectedRow = static_cast<int>(((event->position().y() - m_renderStart.y()) / m_rowHeight)) + m_rowStart;

	// For 128 bit types, support selecting segments
	if (m_cpu->getRegisterSize(categoryIndex) == 128)
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

void RegisterWidget::wheelEvent(QWheelEvent* event)
{
	if (event->angleDelta().y() < 0 && m_rowEnd < m_cpu->getRegisterCount(ui.registerTabs->currentIndex()))
	{
		m_rowStart += 1;
	}
	else if (event->angleDelta().y() > 0 && m_rowStart > 0)
	{
		m_rowStart -= 1;
	}

	this->repaint();
}

void RegisterWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
	if (!m_cpu->isAlive())
		return;
	if (m_selectedRow > m_rowEnd) // Unsigned underflow; selectedRow will be > m_rowEnd (technically negative)
		return;
	const int categoryIndex = ui.registerTabs->currentIndex();
	if (m_cpu->getRegisterSize(categoryIndex) == 128)
		contextChangeSegment();
	else
		contextChangeValue();
}

void RegisterWidget::customMenuRequested(QPoint pos)
{
	if (!m_cpu->isAlive())
		return;

	if (m_selectedRow > m_rowEnd) // Unsigned underflow; selectedRow will be > m_rowEnd (technically negative)
		return;

	// Unlike the disassembly widget, we need to create a new context menu every time
	// we show it. Because some register groups are special
	if (!m_contextMenu)
		m_contextMenu = new QMenu(this);
	else
		m_contextMenu->clear();

	const int categoryIndex = ui.registerTabs->currentIndex();

	QAction* action = 0;

	if (categoryIndex == EECAT_FPR)
	{
		m_contextMenu->addAction(action = new QAction(m_showFPRFloat ? tr("View as hex") : tr("View as float")));
		connect(action, &QAction::triggered, this, [this]() { m_showFPRFloat = !m_showFPRFloat; });
		m_contextMenu->addSeparator();
	}

	if (categoryIndex == EECAT_VU0F)
	{
		m_contextMenu->addAction(action = new QAction(m_showVU0FFloat ? tr("View as hex") : tr("View as float")));
		connect(action, &QAction::triggered, this, [this]() { m_showVU0FFloat = !m_showVU0FFloat; });
		m_contextMenu->addSeparator();
	}

	if (m_cpu->getRegisterSize(categoryIndex) == 128)
	{
		m_contextMenu->addAction(action = new QAction(tr("Copy Top Half"), this));
		connect(action, &QAction::triggered, this, &RegisterWidget::contextCopyTop);
		m_contextMenu->addAction(action = new QAction(tr("Copy Bottom Half"), this));
		connect(action, &QAction::triggered, this, &RegisterWidget::contextCopyBottom);
		m_contextMenu->addAction(action = new QAction(tr("Copy Segment"), this));
		connect(action, &QAction::triggered, this, &RegisterWidget::contextCopySegment);
	}
	else
	{
		m_contextMenu->addAction(action = new QAction(tr("Copy Value"), this));
		connect(action, &QAction::triggered, this, &RegisterWidget::contextCopyValue);
	}

	m_contextMenu->addSeparator();

	if (m_cpu->getRegisterSize(categoryIndex) == 128)
	{
		m_contextMenu->addAction(action = new QAction(tr("Change Top Half"), this));
		connect(action, &QAction::triggered, this, &RegisterWidget::contextChangeTop);
		m_contextMenu->addAction(action = new QAction(tr("Change Bottom Half"), this));
		connect(action, &QAction::triggered, this, &RegisterWidget::contextChangeBottom);
		m_contextMenu->addAction(action = new QAction(tr("Change Segment"), this));
		connect(action, &QAction::triggered, this, &RegisterWidget::contextChangeSegment);
	}
	else
	{
		m_contextMenu->addAction(action = new QAction(tr("Change Value"), this));
		connect(action, &QAction::triggered, this, &RegisterWidget::contextChangeValue);
	}

	m_contextMenu->addSeparator();

	m_contextMenu->addAction(action = new QAction(tr("Go to in Disassembly"), this));
	connect(action, &QAction::triggered, this, &RegisterWidget::contextGotoDisasm);

	m_contextMenu->addAction(action = new QAction(tr("Go to in Memory View"), this));
	connect(action, &QAction::triggered, this, &RegisterWidget::contextGotoMemory);

	m_contextMenu->popup(this->mapToGlobal(pos));
}


void RegisterWidget::contextCopyValue()
{
	const int categoryIndex = ui.registerTabs->currentIndex();
	const u128 val = m_cpu->getRegister(categoryIndex, m_selectedRow);
	if (CAT_SHOW_FLOAT)
		QApplication::clipboard()->setText(QString("%1").arg(QString::number(std::bit_cast<float>(val._u32[0])).toUpper(), 16));
	else
		QApplication::clipboard()->setText(QString("%1").arg(QString::number(val._u64[0], 16).toUpper(), 16));
}

void RegisterWidget::contextCopyTop()
{
	const int categoryIndex = ui.registerTabs->currentIndex();
	const u128 val = m_cpu->getRegister(categoryIndex, m_selectedRow);
	QApplication::clipboard()->setText(FilledQStringFromValue(val.hi, 16));
}

void RegisterWidget::contextCopyBottom()
{
	const int categoryIndex = ui.registerTabs->currentIndex();
	const u128 val = m_cpu->getRegister(categoryIndex, m_selectedRow);
	QApplication::clipboard()->setText(FilledQStringFromValue(val.lo, 16));
}

void RegisterWidget::contextCopySegment()
{
	const int categoryIndex = ui.registerTabs->currentIndex();
	const u128 val = m_cpu->getRegister(categoryIndex, m_selectedRow);
	if (CAT_SHOW_FLOAT)
		QApplication::clipboard()->setText(FilledQStringFromValue(std::bit_cast<float>(val._u32[3 - m_selected128Field]), 10));
	else
		QApplication::clipboard()->setText(FilledQStringFromValue(val._u32[3 - m_selected128Field], 16));
}

bool RegisterWidget::contextFetchNewValue(u64& out, u64 currentValue, bool segment)
{
	const int categoryIndex = ui.registerTabs->currentIndex();
	const bool floatingPoint = CAT_SHOW_FLOAT && segment;
	const int regSize = m_cpu->getRegisterSize(categoryIndex);
	bool ok = false;

	QString existingValue("%1");

	if (!floatingPoint)
		existingValue = existingValue.arg(currentValue, regSize == 64 ? 16 : 8, 16, QChar('0'));
	else
		existingValue = existingValue.arg(std::bit_cast<float>((u32)currentValue));

	//: Changing the value in a CPU register (e.g. "Change t0")
	QString input = QInputDialog::getText(this, tr("Change %1").arg(m_cpu->getRegisterName(categoryIndex, m_selectedRow)), "",
		QLineEdit::Normal, existingValue, &ok);

	if (!ok)
		return false;

	if (!floatingPoint) // Get input as hexadecimal
	{
		out = input.toULongLong(&ok, 16);
		if (!ok)
		{
			QMessageBox::warning(this, tr("Invalid register value"), tr("Invalid hexadecimal register value."));
			return false;
		}
	}
	else
	{
		out = std::bit_cast<u32>(input.toFloat(&ok));
		if (!ok)
		{
			QMessageBox::warning(this, tr("Invalid register value"), tr("Invalid floating-point register value."));
			return false;
		}
	}

	return true;
}

void RegisterWidget::contextChangeValue()
{
	const int categoryIndex = ui.registerTabs->currentIndex();
	u64 newVal;
	if (contextFetchNewValue(newVal, m_cpu->getRegister(categoryIndex, m_selectedRow).lo))
	{
		m_cpu->setRegister(categoryIndex, m_selectedRow, u128::From64(newVal));
		VMUpdate();
	}
}

void RegisterWidget::contextChangeTop()
{
	u64 newVal;
	u128 oldVal = m_cpu->getRegister(ui.registerTabs->currentIndex(), m_selectedRow);
	if (contextFetchNewValue(newVal, oldVal.hi))
	{
		oldVal.hi = newVal;
		m_cpu->setRegister(ui.registerTabs->currentIndex(), m_selectedRow, oldVal);
		VMUpdate();
	}
}

void RegisterWidget::contextChangeBottom()
{
	u64 newVal;
	u128 oldVal = m_cpu->getRegister(ui.registerTabs->currentIndex(), m_selectedRow);
	if (contextFetchNewValue(newVal, oldVal.lo))
	{
		oldVal.lo = newVal;
		m_cpu->setRegister(ui.registerTabs->currentIndex(), m_selectedRow, oldVal);
		VMUpdate();
	}
}

void RegisterWidget::contextChangeSegment()
{
	u64 newVal;
	u128 oldVal = m_cpu->getRegister(ui.registerTabs->currentIndex(), m_selectedRow);
	if (contextFetchNewValue(newVal, oldVal._u32[3 - m_selected128Field], true))
	{
		oldVal._u32[3 - m_selected128Field] = (u32)newVal;
		m_cpu->setRegister(ui.registerTabs->currentIndex(), m_selectedRow, oldVal);
		VMUpdate();
	}
}

void RegisterWidget::contextGotoDisasm()
{
	const int categoryIndex = ui.registerTabs->currentIndex();
	u128 regVal = m_cpu->getRegister(categoryIndex, m_selectedRow);
	u32 addr = 0;

	if (m_cpu->getRegisterSize(categoryIndex) == 128)
		addr = regVal._u32[3 - m_selected128Field];
	else
		addr = regVal._u32[0];

	if (m_cpu->isValidAddress(addr))
		gotoInDisasm(addr);
	else
		QMessageBox::warning(this, tr("Invalid target address"), ("This register holds an invalid address."));
}

void RegisterWidget::contextGotoMemory()
{
	const int categoryIndex = ui.registerTabs->currentIndex();
	u128 regVal = m_cpu->getRegister(categoryIndex, m_selectedRow);
	u32 addr = 0;

	if (m_cpu->getRegisterSize(categoryIndex) == 128)
		addr = regVal._u32[3 - m_selected128Field];
	else
		addr = regVal._u32[0];

	gotoInMemory(addr);
}
