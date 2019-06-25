#include "InGameMenu.h"

#include <boglfw/GUI/ListLayout.h>
#include <boglfw/GUI/controls/Button.h>

InGameMenu::InGameMenu() {
	setClientArea({400, 20}, {400, 20});
	auto layout = std::make_shared<ListLayout>();
	layout->setItemSpacing(30);
	layout->setAlignment(ListLayout::CENTER);			// center items horizontally
	layout->setVerticalAlignment(ListLayout::MIDDLE);	// center contents vertically
	useLayout(layout);

	addButton("Resume", onBack);
	addButton("Quit", onQuit);
}

void InGameMenu::addButton(const char* text, Event<void()> &onClick) {
	auto pBut = std::make_shared<Button>(text);
	pBut->onClick.forward(onClick);
	addElement(pBut);
}
