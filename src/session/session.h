#ifndef SESSION_H
#define SESSION_H

#include <boglfw/renderOpenGL/drawable.h>
#include <boglfw/utils/updatable.h>
#include <boglfw/utils/Event.h>

#include <vector>
#include <memory>

class Terrain;
class Water;
class FreeCamera;
class PlayerEntity;
class CameraController;

class Session {
public:
	enum SessionType {
		LOBBY,		// main menu before a game session is established
		HOST_SETUP,	// host game setup screen
		JOIN_SELECT,// select a game to join from a list or by entering IP
		JOIN_WAIT,	// after joining a session that is being setup by the host
		GAME,		// during game play, after the session is started by the host
		EXIT_GAME,
	};

	Session(SessionType type);
	~Session();

	SessionType type() const { return type_; }

	std::vector<drawable> & drawList3D() { return drawList3D_; }
	std::vector<drawable> & drawList2D() { return drawList2D_; }

	std::weak_ptr<FreeCamera> freeCam() const { return freeCam_; }
	std::weak_ptr<PlayerEntity> player() const { return player_; }
	std::weak_ptr<CameraController> cameraCtrl() const { return cameraCtrl_; }

	void update(float dt);

	Event<void(SessionType)> onNewSessionRequest;

	bool enableWaterRender_ = false;
	Terrain* pTerrain_ = nullptr;
	Water* pWater_ = nullptr;

	static Session* createLobbySession();
	static Session* createHostSession();
	static Session* createJoinSelectSession();
	static Session* createJoinSession();
	static Session* createGameSession();

private:
	SessionType type_;
	std::vector<drawable> drawList3D_;
	std::vector<drawable> drawList2D_;

	std::weak_ptr<FreeCamera> freeCam_;
	std::weak_ptr<PlayerEntity> player_;
	std::weak_ptr<CameraController> cameraCtrl_;
};

#endif // SESSION_H