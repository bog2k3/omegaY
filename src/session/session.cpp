#include "session.h"

#include "../GUI/MainMenu.h"

#include <boglfw/World.h>
#include <boglfw/GUI/GuiSystem.h>

Session* Session::createLobbySession() {
	Session* s = new Session(Session::LOBBY);

	auto guiSystem = World::getGlobal<GuiSystem>();
	auto mainMenu = std::make_shared<MainMenu>(guiSystem->getViewportSize());
	guiSystem->addElement(mainMenu);

	mainMenu->onHostMulti.add([s]() {
		s->onNewSessionRequest.trigger(Session::HOST_SETUP);
	});
	mainMenu->onJoinMulti.add([s]() {
		s->onNewSessionRequest.trigger(Session::JOIN_SELECT);
	});
	mainMenu->onExit.add([s]() {
		s->onNewSessionRequest.trigger(Session::EXIT_GAME);
	});

	return s;
}

Session* Session::createHostSession() {
	Session* s = new Session(Session::HOST_SETUP);
	return s;
}

Session* Session::createJoinSelectSession() {
	Session* s = new Session(Session::JOIN_SELECT);
	return s;
}

Session* Session::createJoinSession() {
	Session* s = new Session(Session::JOIN_WAIT);
	return s;
}

Session* Session::createGameSession() {
	Session* s = new Session(Session::GAME);
	s->enableWaterRender_ = true;
	return s;
}

Session::Session(SessionType type)
	: type_(type) {
}

Session::~Session() {
	World::getGlobal<GuiSystem>()->clear();
}

void Session::update(float dt) {

}