#pragma once

#include "Scene.hpp"
#include "Sound.hpp"

#include <glm/glm.hpp>
#include <random>

using namespace glm;

struct Enemy {
	Enemy(Scene &scene, vec3 pos = vec3(0,0,0));

	enum Direction {
		POS_Z = 0,
		NEG_Z = 1,
		POS_X = 2,
		NEG_X = 3,
		POS_Y = 4,
		NEG_Y = 5
	};

	bool update(float elapsed, vec3 player_pos, std::mt19937 &rnd);

	Scene::Transform * transform = nullptr;
	Scene::Object * object = nullptr;
	Direction dir;

	std::shared_ptr< Sound::PlayingSample > loop;

	float time_to_change = 0.0f;
};
