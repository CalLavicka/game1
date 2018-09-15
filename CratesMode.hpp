#pragma once

#include "Mode.hpp"

#include "MeshBuffer.hpp"
#include "GL.hpp"
#include "Scene.hpp"
#include "Sound.hpp"
#include "WalkMesh.hpp"
#include "Enemy.hpp"

#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vector>

#define MAP_WIDTH 20
#define MAP_HEIGHT 20
#define VERTS_WIDTH 21
#define VERTS_HEIGHT 21

#define MAP_LEVELS 10

// The 'CratesMode' shows scene with some crates in it:

struct CratesMode : public Mode {
	CratesMode();
	virtual ~CratesMode();

	//handle_event is called when new mouse or keyboard events are received:
	// (note that this might be many times per frame or never)
	//The function should return 'true' if it handled the event.
	virtual bool handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) override;

	//update is called at the start of a new frame, after events are handled:
	virtual void update(float elapsed) override;

	//draw is called after update:
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//starts up a 'quit/resume' pause menu:
	void show_pause_menu();

	struct {
		bool forward = false;
		bool backward = false;
		bool left = false;
		bool right = false;
	} controls;

	bool mouse_captured = false;

	Scene scene;
	Scene::Camera *camera = nullptr;

	WalkMesh walk_mesh = WalkMesh({}, {});
	WalkMesh::WalkPoint walk_point;

	float camera_yaw = 0.0f;
	float camera_pitch = 0.0f;

	std::mt19937 random_gen;

	enum Direction : char {
		LEFT = 'l', RIGHT = 'r', UP = 'u', DOWN = 'd'
	};

	static Direction rev_direction(Direction dir) {
		switch(dir) {
		case LEFT:
			return RIGHT;
		case RIGHT:
			return LEFT;
		case UP:
			return DOWN;
		case DOWN:
			return UP;
		default:
			return DOWN; // Should not reach
		}
	}

	enum PlatformType : char {
		NONE = '0', FLAT = 'F', SLOPE = 'S'
	};

	std::vector<Scene::Object *> platforms;
	PlatformType platform_types[MAP_WIDTH][MAP_HEIGHT][MAP_LEVELS];
	
	std::vector<Scene::Object *> buttons;
	std::vector<Enemy> enemies;

	//when this reaches zero, the 'dot' sample is triggered at the small crate:
	float dot_countdown = 1.0f;

	//this 'loop' sample is played at the large crate:
	std::shared_ptr< Sound::PlayingSample > loop;
};
