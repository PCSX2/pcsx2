// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "QtHost.h"

#include "pcsx2/Config.h"
#include "pcsx2/Host.h"

#include "common/Path.h"

#include <QtCore/QFile>
#include <QtGui/QPalette>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleFactory>

namespace QtHost
{
	static void SetStyleFromSettings();
} // namespace QtHost

static QString s_unthemed_style_name;
static QPalette s_unthemed_palette;
static bool s_unthemed_style_name_set;

const char* QtHost::GetDefaultThemeName()
{
#ifdef __APPLE__
	return "";
#else
	return "darkfusion";
#endif
}

void QtHost::UpdateApplicationTheme()
{
	if (!s_unthemed_style_name_set)
	{
		s_unthemed_style_name_set = true;
		s_unthemed_style_name = QApplication::style()->objectName();
		s_unthemed_palette = QApplication::palette();
	}

	SetStyleFromSettings();
	SetIconThemeFromStyle();
}

bool QtHost::IsDarkApplicationTheme()
{
	QPalette palette = qApp->palette();
	return (palette.windowText().color().value() > palette.window().color().value());
}

void QtHost::SetIconThemeFromStyle()
{
	const bool dark = IsDarkApplicationTheme();
	QIcon::setThemeName(dark ? QStringLiteral("white") : QStringLiteral("black"));
}

void QtHost::SetStyleFromSettings()
{
	const std::string theme(Host::GetBaseStringSettingValue("UI", "Theme", GetDefaultThemeName()));

	if (theme == "fusion")
	{
		qApp->setStyle(QStyleFactory::create("Fusion"));
		qApp->setPalette(s_unthemed_palette);
		qApp->setStyleSheet(QString());
	}
#ifdef _WIN32
	else if (theme == "windowsvista")
	{
		qApp->setStyle(QStyleFactory::create("windowsvista"));
		qApp->setPalette(s_unthemed_palette);
		qApp->setStyleSheet(QString());
	}
#endif
	else if (theme == "darkfusion")
	{
		// adapted from https://gist.github.com/QuantumCD/6245215
		qApp->setStyle(QStyleFactory::create("Fusion"));

		const QColor lighterGray(75, 75, 75);
		const QColor darkGray(53, 53, 53);
		const QColor gray(128, 128, 128);
		const QColor black(25, 25, 25);
		const QColor blue(198, 238, 255);

		QPalette darkPalette;
		darkPalette.setColor(QPalette::Window, darkGray);
		darkPalette.setColor(QPalette::WindowText, Qt::white);
		darkPalette.setColor(QPalette::Base, black);
		darkPalette.setColor(QPalette::AlternateBase, darkGray);
		darkPalette.setColor(QPalette::ToolTipBase, darkGray);
		darkPalette.setColor(QPalette::ToolTipText, Qt::white);
		darkPalette.setColor(QPalette::Text, Qt::white);
		darkPalette.setColor(QPalette::Button, darkGray);
		darkPalette.setColor(QPalette::ButtonText, Qt::white);
		darkPalette.setColor(QPalette::Link, blue);
		darkPalette.setColor(QPalette::Highlight, lighterGray);
		darkPalette.setColor(QPalette::HighlightedText, Qt::white);
		darkPalette.setColor(QPalette::PlaceholderText, QColor(Qt::white).darker());

		darkPalette.setColor(QPalette::Active, QPalette::Button, darkGray);
		darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, gray);
		darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, gray);
		darkPalette.setColor(QPalette::Disabled, QPalette::Text, gray);
		darkPalette.setColor(QPalette::Disabled, QPalette::Light, darkGray);

		qApp->setPalette(darkPalette);
		qApp->setStyleSheet(QString());
	}
	else if (theme == "darkfusionblue")
	{
		// adapted from https://gist.github.com/QuantumCD/6245215
		qApp->setStyle(QStyleFactory::create("Fusion"));

		const QColor darkGray(53, 53, 53);
		const QColor gray(128, 128, 128);
		const QColor black(25, 25, 25);
		const QColor blue(198, 238, 255);
		const QColor blue2(0, 88, 208);

		QPalette darkBluePalette;
		darkBluePalette.setColor(QPalette::Window, darkGray);
		darkBluePalette.setColor(QPalette::WindowText, Qt::white);
		darkBluePalette.setColor(QPalette::Base, black);
		darkBluePalette.setColor(QPalette::AlternateBase, darkGray);
		darkBluePalette.setColor(QPalette::ToolTipBase, blue2);
		darkBluePalette.setColor(QPalette::ToolTipText, Qt::white);
		darkBluePalette.setColor(QPalette::Text, Qt::white);
		darkBluePalette.setColor(QPalette::Button, darkGray);
		darkBluePalette.setColor(QPalette::ButtonText, Qt::white);
		darkBluePalette.setColor(QPalette::Link, blue);
		darkBluePalette.setColor(QPalette::Highlight, blue2);
		darkBluePalette.setColor(QPalette::HighlightedText, Qt::white);
		darkBluePalette.setColor(QPalette::PlaceholderText, QColor(Qt::white).darker());

		darkBluePalette.setColor(QPalette::Active, QPalette::Button, darkGray);
		darkBluePalette.setColor(QPalette::Disabled, QPalette::ButtonText, gray);
		darkBluePalette.setColor(QPalette::Disabled, QPalette::WindowText, gray);
		darkBluePalette.setColor(QPalette::Disabled, QPalette::Text, gray);
		darkBluePalette.setColor(QPalette::Disabled, QPalette::Light, darkGray);

		qApp->setPalette(darkBluePalette);
		qApp->setStyleSheet(QString());
	}
	else if (theme == "GreyMatter")
	{
		// Custom palette by KamFretoZ, A sleek and stylish gray
		// that are meant to be easy on the eyes as the main color.
		// Alternative dark theme.
		qApp->setStyle(QStyleFactory::create("Fusion"));

		const QColor darkGray(46, 52, 64);
		const QColor lighterGray(59, 66, 82);
		const QColor gray(111, 111, 111);
		const QColor blue(198, 238, 255);

		QPalette greyMatterPalette;
		greyMatterPalette.setColor(QPalette::Window, darkGray);
		greyMatterPalette.setColor(QPalette::WindowText, Qt::white);
		greyMatterPalette.setColor(QPalette::Base, lighterGray);
		greyMatterPalette.setColor(QPalette::AlternateBase, darkGray);
		greyMatterPalette.setColor(QPalette::ToolTipBase, darkGray);
		greyMatterPalette.setColor(QPalette::ToolTipText, Qt::white);
		greyMatterPalette.setColor(QPalette::Text, Qt::white);
		greyMatterPalette.setColor(QPalette::Button, lighterGray);
		greyMatterPalette.setColor(QPalette::ButtonText, Qt::white);
		greyMatterPalette.setColor(QPalette::Link, blue);
		greyMatterPalette.setColor(QPalette::Highlight, lighterGray.lighter());
		greyMatterPalette.setColor(QPalette::HighlightedText, Qt::white);
		greyMatterPalette.setColor(QPalette::PlaceholderText, QColor(Qt::white).darker());

		greyMatterPalette.setColor(QPalette::Active, QPalette::Button, lighterGray);
		greyMatterPalette.setColor(QPalette::Disabled, QPalette::ButtonText, gray.lighter());
		greyMatterPalette.setColor(QPalette::Disabled, QPalette::WindowText, gray.lighter());
		greyMatterPalette.setColor(QPalette::Disabled, QPalette::Text, gray.lighter());
		greyMatterPalette.setColor(QPalette::Disabled, QPalette::Light, darkGray);

		qApp->setPalette(greyMatterPalette);
		qApp->setStyleSheet(QString());
	}
	else if (theme == "UntouchedLagoon")
	{
		// Custom palette by RedDevilus, Tame (Light/Washed out) Green as main color and Grayish Blue as complimentary.
		// Alternative white theme.
		qApp->setStyle(QStyleFactory::create("Fusion"));

		const QColor black(25, 25, 25);
		const QColor darkteal(0, 77, 77);
		const QColor teal(0, 128, 128);
		const QColor tameTeal(160, 190, 185);
		const QColor grayBlue(160, 180, 190);

		QPalette untouchedLagoonPalette;
		untouchedLagoonPalette.setColor(QPalette::Window, tameTeal);
		untouchedLagoonPalette.setColor(QPalette::WindowText, black.lighter());
		untouchedLagoonPalette.setColor(QPalette::Base, grayBlue);
		untouchedLagoonPalette.setColor(QPalette::AlternateBase, tameTeal);
		untouchedLagoonPalette.setColor(QPalette::ToolTipBase, tameTeal);
		untouchedLagoonPalette.setColor(QPalette::ToolTipText, grayBlue);
		untouchedLagoonPalette.setColor(QPalette::Text, black);
		untouchedLagoonPalette.setColor(QPalette::Button, tameTeal);
		untouchedLagoonPalette.setColor(QPalette::ButtonText, black);
		untouchedLagoonPalette.setColor(QPalette::Link, black.lighter());
		untouchedLagoonPalette.setColor(QPalette::Highlight, teal);
		untouchedLagoonPalette.setColor(QPalette::HighlightedText, grayBlue.lighter());

		untouchedLagoonPalette.setColor(QPalette::Active, QPalette::Button, tameTeal);
		untouchedLagoonPalette.setColor(QPalette::Disabled, QPalette::ButtonText, darkteal);
		untouchedLagoonPalette.setColor(QPalette::Disabled, QPalette::WindowText, darkteal.lighter());
		untouchedLagoonPalette.setColor(QPalette::Disabled, QPalette::Text, darkteal.lighter());
		untouchedLagoonPalette.setColor(QPalette::Disabled, QPalette::Light, tameTeal);

		qApp->setPalette(untouchedLagoonPalette);
		qApp->setStyleSheet(QString());
	}
	else if (theme == "BabyPastel")
	{
		// Custom palette by RedDevilus, Blue as main color and blue as complimentary.
		// Alternative light theme.
		qApp->setStyle(QStyleFactory::create("Fusion"));

		const QColor gray(150, 150, 150);
		const QColor black(25, 25, 25);
		const QColor redpinkish(200, 75, 132);
		const QColor pink(255, 174, 201);
		const QColor brightPink(255, 230, 255);
		const QColor congoPink(255, 127, 121);
		const QColor blue(221, 225, 239);

		QPalette babyPastelPalette;
		babyPastelPalette.setColor(QPalette::Window, pink);
		babyPastelPalette.setColor(QPalette::WindowText, black);
		babyPastelPalette.setColor(QPalette::Base, brightPink);
		babyPastelPalette.setColor(QPalette::AlternateBase, blue);
		babyPastelPalette.setColor(QPalette::ToolTipBase, pink);
		babyPastelPalette.setColor(QPalette::ToolTipText, brightPink);
		babyPastelPalette.setColor(QPalette::Text, black);
		babyPastelPalette.setColor(QPalette::Button, pink);
		babyPastelPalette.setColor(QPalette::ButtonText, black);
		babyPastelPalette.setColor(QPalette::Link, black);
		babyPastelPalette.setColor(QPalette::Highlight, congoPink);
		babyPastelPalette.setColor(QPalette::HighlightedText, black);

		babyPastelPalette.setColor(QPalette::Active, QPalette::Button, pink);
		babyPastelPalette.setColor(QPalette::Disabled, QPalette::ButtonText, redpinkish);
		babyPastelPalette.setColor(QPalette::Disabled, QPalette::WindowText, redpinkish);
		babyPastelPalette.setColor(QPalette::Disabled, QPalette::Text, redpinkish);
		babyPastelPalette.setColor(QPalette::Disabled, QPalette::Light, gray);

		qApp->setPalette(babyPastelPalette);
		qApp->setStyleSheet(QString());
	}
	else if (theme == "PizzaBrown")
	{
		// Custom palette by KamFretoZ, a Pizza Tower Reference!
		// With a mixtures of Light Brown, Peachy/Creamy White, Latte-like Color.
		// Thanks to Jordan for the idea :P
		// Alternative light theme.
		qApp->setStyle(QStyleFactory::create("Fusion"));

		const QColor gray(128, 128, 128);
		const QColor extr(248, 192, 88);
		const QColor main(233, 187, 147);
		const QColor comp(248, 230, 213);
		const QColor highlight(188, 100, 60);

		QPalette pizzaPalette;
		pizzaPalette.setColor(QPalette::Window, main);
		pizzaPalette.setColor(QPalette::WindowText, Qt::black);
		pizzaPalette.setColor(QPalette::Base, comp);
		pizzaPalette.setColor(QPalette::AlternateBase, extr);
		pizzaPalette.setColor(QPalette::ToolTipBase, comp);
		pizzaPalette.setColor(QPalette::ToolTipText, Qt::black);
		pizzaPalette.setColor(QPalette::Text, Qt::black);
		pizzaPalette.setColor(QPalette::Button, extr);
		pizzaPalette.setColor(QPalette::ButtonText, Qt::black);
		pizzaPalette.setColor(QPalette::Link, highlight.darker());
		pizzaPalette.setColor(QPalette::Highlight, highlight);
		pizzaPalette.setColor(QPalette::HighlightedText, Qt::white);
		
		pizzaPalette.setColor(QPalette::Active, QPalette::Button, extr);
		pizzaPalette.setColor(QPalette::Disabled, QPalette::ButtonText, gray.darker());
		pizzaPalette.setColor(QPalette::Disabled, QPalette::WindowText, gray.darker());
		pizzaPalette.setColor(QPalette::Disabled, QPalette::Text, Qt::gray);
		pizzaPalette.setColor(QPalette::Disabled, QPalette::Light, gray.lighter());

		qApp->setPalette(pizzaPalette);
		qApp->setStyleSheet(QString());
	}
	else if (theme == "PCSX2Blue")
	{
		// Custom palette by RedDevilus, White as main color and Blue as complimentary.
		// Alternative light theme.
		qApp->setStyle(QStyleFactory::create("Fusion"));

		const QColor blackish(35, 35, 35);
		const QColor darkBlue(73, 97, 177);
		const QColor blue2(80, 120, 200);
		const QColor blue(106, 156, 255);
		const QColor lightBlue(130, 155, 241);

		QPalette pcsx2BluePalette;
		pcsx2BluePalette.setColor(QPalette::Window, blue2.lighter());
		pcsx2BluePalette.setColor(QPalette::WindowText, blackish);
		pcsx2BluePalette.setColor(QPalette::Base, lightBlue);
		pcsx2BluePalette.setColor(QPalette::AlternateBase, blue2.lighter());
		pcsx2BluePalette.setColor(QPalette::ToolTipBase, blue2);
		pcsx2BluePalette.setColor(QPalette::ToolTipText, Qt::white);
		pcsx2BluePalette.setColor(QPalette::Text, blackish);
		pcsx2BluePalette.setColor(QPalette::Button, blue);
		pcsx2BluePalette.setColor(QPalette::ButtonText, blackish);
		pcsx2BluePalette.setColor(QPalette::Link, darkBlue);
		pcsx2BluePalette.setColor(QPalette::Highlight, Qt::white);
		pcsx2BluePalette.setColor(QPalette::HighlightedText, blackish);

		pcsx2BluePalette.setColor(QPalette::Active, QPalette::Button, blue);
		pcsx2BluePalette.setColor(QPalette::Disabled, QPalette::ButtonText, darkBlue);
		pcsx2BluePalette.setColor(QPalette::Disabled, QPalette::WindowText, darkBlue);
		pcsx2BluePalette.setColor(QPalette::Disabled, QPalette::Text, darkBlue);
		pcsx2BluePalette.setColor(QPalette::Disabled, QPalette::Light, darkBlue);

		qApp->setPalette(pcsx2BluePalette);
		qApp->setStyleSheet(QString());
	}
	else if (theme == "ScarletDevilRed")
	{
		// Custom palette by RedDevilus, Red as main color and Purple as complimentary.
		// Alternative dark theme.
		qApp->setStyle(QStyleFactory::create("Fusion"));

		const QColor darkRed(80, 45, 69);
		const QColor purplishRed(120, 45, 69);
		const QColor brightRed(200, 45, 69);

		QPalette scarletDevilPalette;
		scarletDevilPalette.setColor(QPalette::Window, darkRed);
		scarletDevilPalette.setColor(QPalette::WindowText, Qt::white);
		scarletDevilPalette.setColor(QPalette::Base, purplishRed);
		scarletDevilPalette.setColor(QPalette::AlternateBase, darkRed);
		scarletDevilPalette.setColor(QPalette::ToolTipBase, darkRed);
		scarletDevilPalette.setColor(QPalette::ToolTipText, Qt::white);
		scarletDevilPalette.setColor(QPalette::Text, Qt::white);
		scarletDevilPalette.setColor(QPalette::Button, purplishRed.darker());
		scarletDevilPalette.setColor(QPalette::ButtonText, Qt::white);
		scarletDevilPalette.setColor(QPalette::Link, brightRed);
		scarletDevilPalette.setColor(QPalette::Highlight, brightRed);
		scarletDevilPalette.setColor(QPalette::HighlightedText, Qt::white);

		scarletDevilPalette.setColor(QPalette::Active, QPalette::Button, purplishRed.darker());
		scarletDevilPalette.setColor(QPalette::Disabled, QPalette::ButtonText, brightRed);
		scarletDevilPalette.setColor(QPalette::Disabled, QPalette::WindowText, brightRed);
		scarletDevilPalette.setColor(QPalette::Disabled, QPalette::Text, brightRed);
		scarletDevilPalette.setColor(QPalette::Disabled, QPalette::Light, darkRed);

		qApp->setPalette(scarletDevilPalette);
		qApp->setStyleSheet(QString());
	}
	else if (theme == "VioletAngelPurple")
	{
		// Custom palette by RedDevilus, Blue as main color and Purple as complimentary.
		// Alternative dark theme.
		qApp->setStyle(QStyleFactory::create("Fusion"));

		const QColor blackishblue(50, 25, 70);
		const QColor darkerPurple(90, 30, 105);
		const QColor nauticalPurple(110, 30, 125);

		QPalette violetAngelPalette;
		violetAngelPalette.setColor(QPalette::Window, blackishblue);
		violetAngelPalette.setColor(QPalette::WindowText, Qt::white);
		violetAngelPalette.setColor(QPalette::Base, nauticalPurple);
		violetAngelPalette.setColor(QPalette::AlternateBase, blackishblue);
		violetAngelPalette.setColor(QPalette::ToolTipBase, nauticalPurple);
		violetAngelPalette.setColor(QPalette::ToolTipText, Qt::white);
		violetAngelPalette.setColor(QPalette::Text, Qt::white);
		violetAngelPalette.setColor(QPalette::Button, nauticalPurple.darker());
		violetAngelPalette.setColor(QPalette::ButtonText, Qt::white);
		violetAngelPalette.setColor(QPalette::Link, darkerPurple.lighter());
		violetAngelPalette.setColor(QPalette::Highlight, darkerPurple.lighter());
		violetAngelPalette.setColor(QPalette::HighlightedText, Qt::white);

		violetAngelPalette.setColor(QPalette::Active, QPalette::Button, nauticalPurple.darker());
		violetAngelPalette.setColor(QPalette::Disabled, QPalette::ButtonText, darkerPurple.lighter());
		violetAngelPalette.setColor(QPalette::Disabled, QPalette::WindowText, darkerPurple.lighter());
		violetAngelPalette.setColor(QPalette::Disabled, QPalette::Text, darkerPurple.darker());
		violetAngelPalette.setColor(QPalette::Disabled, QPalette::Light, nauticalPurple);

		qApp->setPalette(violetAngelPalette);
		qApp->setStyleSheet(QString());
	}
	else if (theme == "CobaltSky")
	{
		// Custom palette by KamFretoZ, A soothing deep royal blue
		// that are meant to be easy on the eyes as the main color.
		// Alternative dark theme.
		qApp->setStyle(QStyleFactory::create("Fusion"));

		const QColor gray(150, 150, 150);
		const QColor royalBlue(29, 41, 81);
		const QColor darkishBlue(17, 30, 108);
		const QColor lighterBlue(25, 32, 130);
		const QColor highlight(36, 93, 218);
		const QColor link(0, 202, 255);

		QPalette cobaltSkyPalette;
		cobaltSkyPalette.setColor(QPalette::Window, royalBlue);
		cobaltSkyPalette.setColor(QPalette::WindowText, Qt::white);
		cobaltSkyPalette.setColor(QPalette::Base, royalBlue.lighter());
		cobaltSkyPalette.setColor(QPalette::AlternateBase, darkishBlue);
		cobaltSkyPalette.setColor(QPalette::ToolTipBase, darkishBlue);
		cobaltSkyPalette.setColor(QPalette::ToolTipText, Qt::white);
		cobaltSkyPalette.setColor(QPalette::Text, Qt::white);
		cobaltSkyPalette.setColor(QPalette::Button, lighterBlue);
		cobaltSkyPalette.setColor(QPalette::ButtonText, Qt::white);
		cobaltSkyPalette.setColor(QPalette::Link, link);
		cobaltSkyPalette.setColor(QPalette::Highlight, highlight);
		cobaltSkyPalette.setColor(QPalette::HighlightedText, Qt::white);

		cobaltSkyPalette.setColor(QPalette::Active, QPalette::Button, lighterBlue);
		cobaltSkyPalette.setColor(QPalette::Disabled, QPalette::ButtonText, gray);
		cobaltSkyPalette.setColor(QPalette::Disabled, QPalette::WindowText, gray);
		cobaltSkyPalette.setColor(QPalette::Disabled, QPalette::Text, gray);
		cobaltSkyPalette.setColor(QPalette::Disabled, QPalette::Light, gray);

		qApp->setPalette(cobaltSkyPalette);
		qApp->setStyleSheet(QString());
	}
	else if (theme == "Ruby")
	{
		// Custom palette by Daisouji, Black as main color and Red as complimentary.
		// Alternative dark (black) theme.
		qApp->setStyle(QStyleFactory::create("Fusion"));

		const QColor gray(128, 128, 128);
		const QColor slate(18, 18, 18);
		const QColor rubyish(172, 21, 31);

		QPalette rubyPalette;
		rubyPalette.setColor(QPalette::Window, slate);
		rubyPalette.setColor(QPalette::WindowText, Qt::white);
		rubyPalette.setColor(QPalette::Base, slate.lighter());
		rubyPalette.setColor(QPalette::AlternateBase, slate.lighter());
		rubyPalette.setColor(QPalette::ToolTipBase, slate);
		rubyPalette.setColor(QPalette::ToolTipText, Qt::white);
		rubyPalette.setColor(QPalette::Text, Qt::white);
		rubyPalette.setColor(QPalette::Button, slate);
		rubyPalette.setColor(QPalette::ButtonText, Qt::white);
		rubyPalette.setColor(QPalette::Link, Qt::white);
		rubyPalette.setColor(QPalette::Highlight, rubyish);
		rubyPalette.setColor(QPalette::HighlightedText, Qt::white);

		rubyPalette.setColor(QPalette::Active, QPalette::Button, slate);
		rubyPalette.setColor(QPalette::Disabled, QPalette::ButtonText, gray);
		rubyPalette.setColor(QPalette::Disabled, QPalette::WindowText, gray);
		rubyPalette.setColor(QPalette::Disabled, QPalette::Text, gray);
		rubyPalette.setColor(QPalette::Disabled, QPalette::Light, slate.lighter());

		qApp->setPalette(rubyPalette);
		qApp->setStyleSheet(QString());
	}
	else if (theme == "Sapphire")
	{
		// Custom palette by RedDevilus, Black as main color and Blue as complimentary.
		// Alternative dark (black) theme.
		qApp->setStyle(QStyleFactory::create("Fusion"));

		const QColor gray(128, 128, 128);
		const QColor slate(18, 18, 18);
		const QColor persianBlue(32, 35, 204);

		QPalette sapphirePalette;
		sapphirePalette.setColor(QPalette::Window, slate);
		sapphirePalette.setColor(QPalette::WindowText, Qt::white);
		sapphirePalette.setColor(QPalette::Base, slate.lighter());
		sapphirePalette.setColor(QPalette::AlternateBase, slate.lighter());
		sapphirePalette.setColor(QPalette::ToolTipBase, slate);
		sapphirePalette.setColor(QPalette::ToolTipText, Qt::white);
		sapphirePalette.setColor(QPalette::Text, Qt::white);
		sapphirePalette.setColor(QPalette::Button, slate);
		sapphirePalette.setColor(QPalette::ButtonText, Qt::white);
		sapphirePalette.setColor(QPalette::Link, Qt::white);
		sapphirePalette.setColor(QPalette::Highlight, persianBlue);
		sapphirePalette.setColor(QPalette::HighlightedText, Qt::white);

		sapphirePalette.setColor(QPalette::Active, QPalette::Button, slate);
		sapphirePalette.setColor(QPalette::Disabled, QPalette::ButtonText, gray);
		sapphirePalette.setColor(QPalette::Disabled, QPalette::WindowText, gray);
		sapphirePalette.setColor(QPalette::Disabled, QPalette::Text, gray);
		sapphirePalette.setColor(QPalette::Disabled, QPalette::Light, slate.lighter());

		qApp->setPalette(sapphirePalette);
		qApp->setStyleSheet(QString());
	}
	else if (theme == "Emerald")
	{
		// Custom palette by RedDevilus, Black as main color and Blue as complimentary.
		// Alternative dark (black) theme.
		qApp->setStyle(QStyleFactory::create("Fusion"));

		const QColor gray(128, 128, 128);
		const QColor slate(18, 18, 18);
		const QColor evergreenEmerald(15, 81, 59);

		QPalette emeraldPalette;
		emeraldPalette.setColor(QPalette::Window, slate);
		emeraldPalette.setColor(QPalette::WindowText, Qt::white);
		emeraldPalette.setColor(QPalette::Base, slate.lighter());
		emeraldPalette.setColor(QPalette::AlternateBase, slate.lighter());
		emeraldPalette.setColor(QPalette::ToolTipBase, slate);
		emeraldPalette.setColor(QPalette::ToolTipText, Qt::white);
		emeraldPalette.setColor(QPalette::Text, Qt::white);
		emeraldPalette.setColor(QPalette::Button, slate);
		emeraldPalette.setColor(QPalette::ButtonText, Qt::white);
		emeraldPalette.setColor(QPalette::Link, Qt::white);
		emeraldPalette.setColor(QPalette::Highlight, evergreenEmerald);
		emeraldPalette.setColor(QPalette::HighlightedText, Qt::white);

		emeraldPalette.setColor(QPalette::Active, QPalette::Button, slate);
		emeraldPalette.setColor(QPalette::Disabled, QPalette::ButtonText, gray);
		emeraldPalette.setColor(QPalette::Disabled, QPalette::WindowText, gray);
		emeraldPalette.setColor(QPalette::Disabled, QPalette::Text, gray);
		emeraldPalette.setColor(QPalette::Disabled, QPalette::Light, slate.lighter());

		qApp->setPalette(emeraldPalette);
		qApp->setStyleSheet(QString());
	}
	else if (theme == "Custom")
	{
		//Additional Theme option than loads .qss from main PCSX2 Directory
		qApp->setStyle(QStyleFactory::create("Fusion"));

		QString sheet_content;
		QFile sheets(QString::fromStdString(Path::Combine(EmuFolders::DataRoot, "custom.qss")));

		if (sheets.open(QFile::ReadOnly))
		{
			QString sheet_content = QString::fromUtf8(sheets.readAll().data());
			qApp->setStyleSheet(sheet_content);
		}
		else
		{
			qApp->setStyle(QStyleFactory::create("Fusion"));
		}
	}
	else
	{
		qApp->setStyle(s_unthemed_style_name);
		qApp->setPalette(s_unthemed_palette);
		qApp->setStyleSheet(QString());
	}
}
