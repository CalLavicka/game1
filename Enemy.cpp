#include "Enemy.hpp"

#include "Load.hpp"
#include "Sound.hpp"
#include "MeshBuffer.hpp"
#include "data_path.hpp"
#include "vertex_color_program.hpp"

#include <vector>
#include <iostream>

#define PI 3.14159265f

#define COLLISION_RADIUS_2 0.25f

using namespace glm;

const quat rotations[] = {
	quat(0, 0, 0, 1), angleAxis(PI, vec3(1, 0, 0)),
	angleAxis(PI / 2.f, vec3(0, 1, 0)), angleAxis(PI / 2.f, vec3(0, -1, 0)),
	angleAxis(PI / 2.f, vec3(-1, 0, 0)), angleAxis(PI / 2.f, vec3(1, 0, 0))
};

const vec3 units[] = {
	vec3(0, 0, 1), vec3(0, 0, -1),
	vec3(1, 0, 0), vec3(-1, 0, 0),
	vec3(0, 1, 0), vec3(0, -1, 0)
};

Load< MeshBuffer > enemy_meshes(LoadTagDefault, [](){
	return new MeshBuffer(data_path("enemies.pnc"));
});

Load< GLuint > enemy_meshes_for_vertex_color_program(LoadTagDefault, [](){
	return new GLuint(enemy_meshes->make_vao_for_program(vertex_color_program->program));
});

Enemy::Enemy(Scene &scene, vec3 pos) {

	transform = scene.new_transform();
	transform->position = pos;

	object = scene.new_object(transform);
	object->program = vertex_color_program->program;
	object->program_mvp_mat4 = vertex_color_program->object_to_clip_mat4;
	object->program_mv_mat4x3 = vertex_color_program->object_to_light_mat4x3;
	object->program_itmv_mat3 = vertex_color_program->normal_to_light_mat3;
	object->vao = *enemy_meshes_for_vertex_color_program;
	MeshBuffer::Mesh const &mesh = enemy_meshes->lookup("Enemy");
	object->start = mesh.start;
	object->count = mesh.count;

	transform->scale = vec3(1,1,1);
	dir = POS_X;
}

bool Enemy::update(float elapsed, vec3 player_pos, std::mt19937 &rnd) {
	transform->position += units[dir] * elapsed;
	transform->rotation = rotations[dir];

	time_to_change -= elapsed;
	//std::cout << "CHANGE:" << time_to_change << std::endl;



	vec3 dif = player_pos - transform->position;

	if (time_to_change <= 0.0f) {
		std::uniform_int_distribution<float> distribution(2.f,4.f);
		time_to_change += distribution(rnd);

		float mag = length(dif);
		if (mag > 0) {
			dif /= mag;
		}

		std::vector<Direction> dirs;
		if (dif.x > 0.4)
			dirs.push_back(POS_X);
		if (dif.y > 0.4)
			dirs.push_back(POS_Y);
		if (dif.z > 0.4)
			dirs.push_back(POS_Z);
		if (dif.x < -0.4)
			dirs.push_back(NEG_X);
		if (dif.y < -0.4)
			dirs.push_back(NEG_Y);
		if (dif.z < -0.4)
			dirs.push_back(NEG_Z);
		
		Direction chosen;
		if (dirs.size() > 0) {
			chosen = dirs[rnd() % dirs.size()];
		} else {
			chosen = POS_X;
		}
		std::cout << "NEW DIR:" << chosen << std::endl;
		this->dir = chosen;
	}

	if (dot(dif, dif) < COLLISION_RADIUS_2) {
		return true;
	}
	return false;
}