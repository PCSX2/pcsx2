/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"

#include "MouseWheelFilter.h"

#include <QtCore/QEvent>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpinBox>

MouseWheelFilter::MouseWheelFilter(QObject* parent) : QObject(parent){}

void MouseWheelFilter::install(QObject* target_widget_parent)
{
	QListIterator<QWidget*> WidgetIterator(target_widget_parent->findChildren<QWidget*>());
	while (WidgetIterator.hasNext())
	{
		QWidget* iteration = WidgetIterator.next();
		if(qobject_cast<QComboBox*>(iteration) || qobject_cast<QSpinBox*>(iteration) || qobject_cast<QDoubleSpinBox*>(iteration) || qobject_cast<QSlider*>(iteration))
		 {
			iteration->setFocusPolicy(Qt::StrongFocus);
			iteration->installEventFilter(this);
		 }
	}
}

bool MouseWheelFilter::eventFilter(QObject* object, QEvent* event)
{
	if (event->type() != QEvent::Wheel)
		return false;

	QWidget* widget = qobject_cast<QWidget*>(object);
	if(!widget || widget->hasFocus())
		return false;

	event->ignore();
	return true;
}
