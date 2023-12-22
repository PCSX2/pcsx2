// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <QtWidgets/QProgressDialog>

#include "DEV9/ATA/HddCreate.h"

class HddCreateQt : public HddCreate
{
public:
	HddCreateQt(QWidget* parent);
	virtual ~HddCreateQt(){};

private:
	QWidget* m_parent;
	QProgressDialog* progressDialog;

	int reqMiB;

protected:
	virtual void Init();
	virtual void Cleanup();
	virtual void SetFileProgress(u64 currentSize);
	virtual void SetError();
};
