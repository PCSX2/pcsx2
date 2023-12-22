// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "ColorPickerButton.h"
#include "QtUtils.h"

#include <QtWidgets/QColorDialog>

ColorPickerButton::ColorPickerButton(QWidget* parent)
	: QPushButton(parent)
{
	connect(this, &QPushButton::clicked, this, &ColorPickerButton::onClicked);
	updateBackgroundColor();
}

u32 ColorPickerButton::color()
{
	return m_color;
}

void ColorPickerButton::setColor(u32 rgb)
{
	if (m_color == rgb)
		return;

	m_color = rgb;
	updateBackgroundColor();
}

void ColorPickerButton::updateBackgroundColor()
{
	setStyleSheet(QStringLiteral("background-color: #%1;").arg(static_cast<uint>(m_color), 8, 16, QChar('0')));
}

void ColorPickerButton::onClicked()
{
	const u32 red = (m_color >> 16) & 0xff;
	const u32 green = (m_color >> 8) & 0xff;
	const u32 blue = m_color & 0xff;

	const QColor initial(QColor::fromRgb(red, green, blue));
	const QColor selected(QColorDialog::getColor(initial, QtUtils::GetRootWidget(this), tr("Select LED Color")));

	// QColorDialog returns Invalid on cancel, and apparently initial == Invalid is true...
	if (!selected.isValid() || initial == selected)
		return;

	const u32 new_rgb =
		(static_cast<u32>(selected.red()) << 16) | (static_cast<u32>(selected.green()) << 8) | static_cast<u32>(selected.blue());
	m_color = new_rgb;
	updateBackgroundColor();
	emit colorChanged(new_rgb);
}
