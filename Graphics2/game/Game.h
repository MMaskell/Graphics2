#pragma once
#include "../renderer/Scene.h"
#include "Player.h"
#include "../terrain/Planet.h"
#include "../renderer/Portal.h"
#include <unordered_set>

class Game {
	friend void generateTerrain(Game* game);
	friend void loadAssets(Game* game);
public:
	Game();
	~Game();
	Scene* scene;
	Scene* secondScene;
	Player* player;
	void keyEvent(GLFWwindow* window, int key, int scancode, int action, int mods);
	void update(double dt);
	void draw();
	glm::vec3 worldPos;
	const float lowLodScale = 1.0f/1000.0f;
private:
	Planet* homeWorld;
	Planet* otherWorld;
	Portal* portal;
	Camera* lowLodCam;
	Scene* lowLodScene;
	Scene* secondLowLodScene;
	SceneObject* transformedSpace;
	bool forceVisualUpdate;
	std::unordered_set<Mesh*> highPoly;
	bool inFirstScene = true;
};

