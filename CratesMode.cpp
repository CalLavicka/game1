#include "CratesMode.hpp"

#include "MenuMode.hpp"
#include "Load.hpp"
#include "Sound.hpp"
#include "MeshBuffer.hpp"
#include "gl_errors.hpp" //helper for dumpping OpenGL error messages
#include "read_chunk.hpp" //helper for reading a vector of structures from a file
#include "data_path.hpp" //helper to get paths relative to executable
#include "compile_program.hpp" //helper to compile opengl shader programs
#include "draw_text.hpp" //helper to... um.. draw text
#include "vertex_color_program.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/random.hpp>

#include <iostream>
#include <fstream>
#include <map>
#include <cstddef>
#include <random>
#include <queue>

using namespace glm;

#define PI 3.14159265f

Load< MeshBuffer > platform_meshes(LoadTagDefault, [](){
	return new MeshBuffer(data_path("platforms.pnc"));
});

Load< GLuint > platform_meshes_for_vertex_color_program(LoadTagDefault, [](){
	return new GLuint(platform_meshes->make_vao_for_program(vertex_color_program->program));
});

Load< Sound::Sample > sample_dot(LoadTagDefault, [](){
	return new Sound::Sample(data_path("dot.wav"));
});
Load< Sound::Sample > sample_loop(LoadTagDefault, [](){
	return new Sound::Sample(data_path("loop.wav"));
});

CratesMode::CratesMode() {
	//----------------
	//set up scene:
	//TODO: this should load the scene from a file!

	auto attach_platform_object = [this](Scene::Transform *transform, std::string const &name) {
		Scene::Object *object = scene.new_object(transform);
		object->program = vertex_color_program->program;
		object->program_mvp_mat4 = vertex_color_program->object_to_clip_mat4;
		object->program_mv_mat4x3 = vertex_color_program->object_to_light_mat4x3;
		object->program_itmv_mat3 = vertex_color_program->normal_to_light_mat3;
		object->vao = *platform_meshes_for_vertex_color_program;
		MeshBuffer::Mesh const &mesh = platform_meshes->lookup(name);
		object->start = mesh.start;
		object->count = mesh.count;
		return object;
	};

	{ //Camera looking at the origin:
		Scene::Transform *transform = scene.new_transform();
		transform->position = glm::vec3(0.0f, -10.0f, 1.0f);
		//Cameras look along -z, so rotate view to look at origin:
		transform->rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		camera = scene.new_camera(transform);
	}

	{ //Setup walkmesh, with generation
		std::random_device r;
		std::seed_seq seed{r(), r(), r(), r(), r(), r(), r(), r()};
		std::mt19937 rnd{seed};
		random_gen = rnd;

		std::vector<vec3> verts;

		verts.resize(VERTS_WIDTH * VERTS_HEIGHT * MAP_LEVELS);
		for (int level = 0; level < MAP_LEVELS; level++) {
			for (int j=0; j<VERTS_HEIGHT; j++) {
				for (int i=0; i<VERTS_WIDTH; i++) {
					verts[level * VERTS_WIDTH * VERTS_HEIGHT + j * VERTS_WIDTH + i] = vec3(i, j, level * 0.5f);
				}
			}
		}
		std::vector<uvec3> tris;

		auto add_square = [&tris, attach_platform_object, this](uint32_t square_x, uint32_t square_y, uint32_t level) {
			uvec3 level_add = uvec3(level * VERTS_WIDTH * VERTS_HEIGHT, level * VERTS_WIDTH * VERTS_HEIGHT, level * VERTS_WIDTH * VERTS_HEIGHT); 
			tris.emplace_back(uvec3(square_y * VERTS_WIDTH + square_x, square_y * VERTS_WIDTH + square_x + 1,
							  (square_y + 1) * VERTS_WIDTH + square_x) + level_add);
			tris.emplace_back(uvec3(square_y * VERTS_WIDTH + square_x + 1, (square_y + 1) * VERTS_WIDTH + square_x + 1,
							  (square_y + 1) * VERTS_WIDTH + square_x ) + level_add);

			Scene::Transform *transform = scene.new_transform();
			transform->position = vec3(square_x + 0.5f, square_y + 0.5f, level * 0.5f - 0.1f);
			auto obj = attach_platform_object(transform, "Platform_Base");
			platforms.emplace_back(obj);

			platform_types[square_x][square_y][level] = FLAT;

			std::cout << "ADDED: " << square_x << ", " << square_y << ", " << level << std::endl;

			return obj;
		};
		
		auto add_slope = [&tris, attach_platform_object, this] (uint32_t square_x, uint32_t square_y, uint32_t level, Direction dir) {
			uvec3 level_add1 = uvec3(level * VERTS_WIDTH * VERTS_HEIGHT, level * VERTS_WIDTH * VERTS_HEIGHT, level * VERTS_WIDTH * VERTS_HEIGHT);
			uvec3 level_add2 = level_add1;
			Scene::Transform *transform = scene.new_transform();
			transform->position = vec3(square_x + 0.5f, square_y + 0.5f, level * 0.5f + 0.15f);

			platform_types[square_x][square_y][level] = SLOPE;
			platform_types[square_x][square_y][level + 1] = SLOPE;

			switch(dir) {
			case LEFT:
				level_add1 += uvec3(VERTS_WIDTH * VERTS_HEIGHT, 0, VERTS_WIDTH * VERTS_HEIGHT);
				level_add2 += uvec3(0, 0, VERTS_WIDTH * VERTS_HEIGHT);
				transform->rotation = angleAxis(PI/7.f, vec3(0.f, 1.f, 0.f));
				transform->scale = vec3(1.2f, 1.f, 1.f);
				break;
			case RIGHT:
				level_add1 += uvec3(0, VERTS_WIDTH * VERTS_HEIGHT, 0);
				level_add2 += uvec3(VERTS_WIDTH * VERTS_HEIGHT, VERTS_WIDTH * VERTS_HEIGHT, 0);
				transform->rotation = angleAxis(PI/7.f, vec3(0.f, -1.f, 0.f));
				transform->scale = vec3(1.2f, 1.f, 1.f);
				break;
			case UP:
				level_add1 += uvec3(0, 0, VERTS_WIDTH * VERTS_HEIGHT);
				level_add2 += uvec3(0, VERTS_WIDTH * VERTS_HEIGHT, VERTS_WIDTH * VERTS_HEIGHT);
				transform->rotation = angleAxis(PI/7.f, vec3(1.f, 0.f, 0.f));
				transform->scale = vec3(1.f, 1.2f, 1.f);
				break;
			case DOWN:
				level_add1 += uvec3(VERTS_WIDTH * VERTS_HEIGHT, VERTS_WIDTH * VERTS_HEIGHT, 0);
				level_add2 += uvec3(VERTS_WIDTH * VERTS_HEIGHT, 0, 0);
				transform->rotation = angleAxis(PI/7.f, vec3(-1.f, 0.f, 0.f));
				transform->scale = vec3(1.f, 1.2f, 1.f);
				break;
			};
			tris.emplace_back(uvec3(square_y * VERTS_WIDTH + square_x, square_y * VERTS_WIDTH + square_x + 1,
							  (square_y + 1) * VERTS_WIDTH + square_x) + level_add1);
			tris.emplace_back(uvec3(square_y * VERTS_WIDTH + square_x + 1, (square_y + 1) * VERTS_WIDTH + square_x + 1,
							  (square_y + 1) * VERTS_WIDTH + square_x) + level_add2);
			platforms.emplace_back(attach_platform_object(transform, "Platform_Base"));
		};

		bool map[MAP_WIDTH][MAP_HEIGHT][MAP_LEVELS];
		for (size_t i = 0; i < MAP_WIDTH; i++) {
			for (size_t j = 0; j < MAP_HEIGHT; j++) {
				for (size_t k = 0; k < MAP_LEVELS; k++) {
					map[i][j][k] = false;
					platform_types[i][j][k] = NONE;
				}
			}
		}

		std::vector<uvec3> end_pts;

		auto move_space = [](uint32_t &xpos, uint32_t &ypos, Direction dir) {
			switch(dir) {
			case UP:
				ypos += 1;
				break;
			case DOWN:
				ypos -= 1;
				break;
			case LEFT:
				xpos -= 1;
				break;
			case RIGHT:
				xpos += 1;
				break;
			}
		};

		auto is_wall = [&map](uint32_t xpos, uint32_t ypos, uint32_t zpos) {
			return xpos >= MAP_WIDTH  || ypos >= MAP_HEIGHT || zpos >= MAP_LEVELS
			|| map[xpos][ypos][zpos];
		};
		auto is_free_space = [&map](uint32_t xpos, uint32_t ypos, uint32_t zpos) {
			return xpos >= MAP_WIDTH || ypos >= MAP_HEIGHT || zpos >= MAP_LEVELS
			|| !map[xpos][ypos][zpos];
		};

		auto is_free_move = [is_wall, is_free_space, move_space](uint32_t xpos, uint32_t ypos, uint32_t zpos, Direction dir) {
			
			move_space(xpos, ypos, dir);
			
			// Check for wall, and also check for above/below position
			if (is_wall(xpos, ypos, zpos))// || !is_free_space(xpos, ypos, zpos - 1) || !is_free_space(xpos, ypos, zpos + 1))
				return false;
			
			switch(dir) {
			case UP:
				return is_free_space(xpos - 1, ypos, zpos) && is_free_space(xpos + 1, ypos, zpos)
				&& is_free_space(xpos - 1, ypos + 1, zpos) && is_free_space(xpos, ypos + 1, zpos)
				&& is_free_space(xpos + 1, ypos + 1, zpos);
			case DOWN:
				return is_free_space(xpos - 1, ypos, zpos) && is_free_space(xpos + 1, ypos, zpos)
				&& is_free_space(xpos - 1, ypos - 1, zpos) && is_free_space(xpos, ypos - 1, zpos)
				&& is_free_space(xpos + 1, ypos - 1, zpos);
			case LEFT:
				return is_free_space(xpos - 1, ypos, zpos) && is_free_space(xpos - 1, ypos - 1, zpos)
				&& is_free_space(xpos - 1, ypos + 1, zpos) && is_free_space(xpos, ypos - 1, zpos)
				&& is_free_space(xpos, ypos + 1, zpos);
			case RIGHT:
				return is_free_space(xpos, ypos - 1, zpos) && is_free_space(xpos + 1, ypos, zpos)
				&& is_free_space(xpos + 1, ypos - 1, zpos) && is_free_space(xpos, ypos + 1, zpos)
				&& is_free_space(xpos + 1, ypos + 1, zpos);
			default:
				return false; // Should not reach
			}
		};

		//std::function<void(uint32_t, uint32_t, uint32_t)> gen_step;
		auto gen_func = [&add_square, &add_slope, &is_wall, &is_free_move, &move_space, &map, &end_pts, &rnd]
					(uint32_t xpos, uint32_t ypos, uint32_t zpos) {
			if (is_wall(xpos, ypos, zpos)) {
				return;
			}
			
			// Mark initial spaces
			map[xpos][ypos][zpos] = true;
			if (zpos > 0) map[xpos][ypos][zpos-1] = true;
			if (zpos < MAP_LEVELS - 1) map[xpos][ypos][zpos+1] = true;

			// PERFORM BFS
			std::queue<uvec3> target_spaces;
			target_spaces.push(uvec3(xpos, ypos, zpos));
			
			while(!target_spaces.empty()) {
				uvec3 pt = target_spaces.front();
				target_spaces.pop();
				xpos = pt.x;
				ypos = pt.y;
				zpos = pt.z;

				// Go through all directions
				int paths = 0;
				std::vector<Direction> valid = {UP, DOWN, LEFT, RIGHT};
				while (valid.size() > 0) {
					size_t index = rnd() % valid.size();
					Direction dir = valid[index];
					valid.erase(valid.begin() + index);
					if (is_free_move(xpos, ypos, zpos, dir)) {
						paths++;
						// Get newx and new y
						uint32_t newx = xpos;
						uint32_t newy = ypos;
						move_space(newx, newy, dir);

						// See whether to do elevation
						std::vector<int> elev_change = {-1, 0, 0, 1};
						while(elev_change.size() > 0) {
							int index = rnd() % elev_change.size();
							int choice = elev_change[index];
							elev_change.erase(elev_change.begin() + index);
							if (choice == 0) {

								// Mark as true, also above/below
								map[newx][newy][zpos] = true;
								if (zpos > 0) map[newx][newy][zpos-1] = true;
								if (zpos < MAP_LEVELS - 1) map[newx][newy][zpos+1] = true;
								target_spaces.push(uvec3(newx, newy, zpos));
								break;
							} else {
								// Try and elevate
								uint32_t newz = zpos + choice;
								if (newz < MAP_LEVELS) {
									if (is_free_move(xpos, ypos, newz, dir) && is_free_move(newx, newy, newz, dir)) {
										// Place slope
										if (choice == -1) {
											add_slope(newx, newy, newz, CratesMode::rev_direction(dir));
										} else {
											add_slope(newx, newy, zpos, dir);
										}
										// Fill in map
										map[newx][newy][zpos] = true;
										map[newx][newy][newz] = true;
										if (zpos - choice < MAP_LEVELS) map[newx][newy][zpos - choice] = true;
										if (newz + choice < MAP_LEVELS) map[newx][newy][newz + choice] = true;


										// move x and y again, fill in for new zpos
										move_space(newx, newy, dir);

										map[newx][newy][newz] = true;
										if (newz > 0) map[newx][newy][newz-1] = true;
										if (newz < MAP_LEVELS - 1) map[newx][newy][newz+1] = true;

										target_spaces.push(uvec3(newx, newy, newz));
										break;
									}
								}
							}
						}
					}
				}
				if (paths == 0) {
					end_pts.emplace_back(uvec3(xpos, ypos, zpos));
				} else {
					add_square(xpos, ypos, zpos);
				}
			}
		};
		(void) gen_func;
		(void) add_slope;

		/*		
		add_square(0, 0, 0);
		add_square(0, 1, 0);
		add_square(0, 2, 0);
		add_square(1, 2, 0);
		add_slope(0, 3, 0, UP);
		add_square(0, 4, 1);
		add_square(1, 4, 1);
		add_slope(2, 4, 1, RIGHT);
		add_square(3, 4, 2);

		add_square(1, 0, 0);
		add_square(2, 0, 0);
		add_square(3, 0, 0);
		add_square(3, 1, 0);
		add_square(3, 2, 0);
		add_square(3, 3, 0);
		add_square(3, 4, 0);
		add_slope(3, 5, 0, UP);
		add_slope(3, 6, 1, UP);
		add_square(3, 7, 2);
		add_slope(2, 7, 2, LEFT);
		add_square(1, 7, 3);
		add_slope(1, 6, 3, DOWN);
		add_square(1, 5, 4);
		add_square(2, 5, 4);
		*/
		gen_func(MAP_WIDTH/2, MAP_HEIGHT/2, 2);
		// pick 5 buttons
		int to_place = 5;
		while(end_pts.size() > 0 && to_place > 0) {
			size_t rand_index = rnd() % end_pts.size();
			uvec3 pt = end_pts[rand_index];
			end_pts.erase(end_pts.begin() + rand_index);

			// Place square
			auto object = add_square(pt.x, pt.y, pt.z);
			Scene::Transform *transform = scene.new_transform();
			transform->set_parent(object->transform);
			transform->position = vec3(0.f, 0.f, 0.f);
			buttons.push_back(attach_platform_object(transform, "Button"));

			to_place--;
		}

		// For all other end points, normal platform
		for (auto pt : end_pts) {
			add_square(pt.x, pt.y, pt.z);
		}

		// try to make additional bridges
		for (auto pt : end_pts) {
			Direction dirs[] = {LEFT, RIGHT, UP, DOWN};
			Direction dir = dirs[rnd() % 4];

			// Try to place a space adjacent
			uint32_t targetx = pt.x;
			uint32_t targety = pt.y;
			move_space(targetx, targety, dir);

			uint32_t farx = targetx;
			uint32_t fary = targety;
			move_space(farx, fary, dir);
			if (farx < MAP_WIDTH && fary < MAP_HEIGHT && is_free_space(targetx, targety, pt.z)) {
				// Try to slope up / down;
				uint32_t tries[] = {pt.z, pt.z - 1, pt.z + 1};
				for (uint32_t zpos : tries) {
					if (zpos < MAP_LEVELS && is_free_space(targetx, targety, zpos)
						&& platform_types[farx][fary][zpos] == FLAT) {
						map[targetx][targety][pt.z] = true;
						if (pt.z > 0)
							map[targetx][targety][pt.z-1] = true;
						if (pt.z < MAP_LEVELS - 1)
							map[targetx][targety][pt.z + 1] = true;
						
						if (zpos == pt.z)
							add_square(targetx, targety, zpos);
						else if (zpos < pt.z) {
							add_slope(targetx, targety, zpos, rev_direction(dir));
							if (zpos > 0) {
								map[targetx][targety][zpos - 1] = true;
							}
						} else {
							add_slope(targetx, targety, pt.z, dir);
							if (zpos < MAP_LEVELS - 1) {
								map[targetx][targety][zpos + 1] = true;
							}
						}
					}
				}
			}
		}

		for (auto i = tris.begin(); i != tris.end(); ++i) {
   			std::cout << '[' << i->x << ", " << i->y << ", " << i->z << "] ";
		}
		std::cout << std::endl;

		walk_mesh = WalkMesh(verts, tris);

		walk_point = walk_mesh.start(vec3(MAP_WIDTH/2, MAP_HEIGHT/2, 2));
		camera->transform->position = walk_mesh.world_point(walk_point) + vec3(0, 0, 0.5f);
	}

	{ //Setup enemies
		enemies.emplace_back(scene, vec3(0, 20, 2));
		enemies.emplace_back(scene, vec3(0, 10, 1));
		enemies.emplace_back(scene, vec3(0, 15, 3));
		enemies.emplace_back(scene, vec3(0, 5, 4));
		enemies.emplace_back(scene, vec3(0, 0, 0));
	}
	
	//start the 'loop' sample playing at the large crate:
	//loop = sample_loop->play(large_crate->transform->position, 1.0f, Sound::Loop);
}

CratesMode::~CratesMode() {
	if (loop) loop->stop();
}

bool CratesMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	//ignore any keys that are the result of automatic key repeat:
	if (evt.type == SDL_KEYDOWN && evt.key.repeat) {
		return false;
	}
	//handle tracking the state of WSAD for movement control:
	if (evt.type == SDL_KEYDOWN || evt.type == SDL_KEYUP) {
		if (evt.key.keysym.scancode == SDL_SCANCODE_W) {
			controls.forward = (evt.type == SDL_KEYDOWN);
			return true;
		} else if (evt.key.keysym.scancode == SDL_SCANCODE_S) {
			controls.backward = (evt.type == SDL_KEYDOWN);
			return true;
		} else if (evt.key.keysym.scancode == SDL_SCANCODE_A) {
			controls.left = (evt.type == SDL_KEYDOWN);
			return true;
		} else if (evt.key.keysym.scancode == SDL_SCANCODE_D) {
			controls.right = (evt.type == SDL_KEYDOWN);
			return true;
		}
	}
	//handle tracking the mouse for rotation control:
	if (!mouse_captured) {
		if (evt.type == SDL_KEYDOWN && evt.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
			Mode::set_current(nullptr);
			return true;
		}
		if (evt.type == SDL_MOUSEBUTTONDOWN) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			mouse_captured = true;
			return true;
		}
	} else if (mouse_captured) {
		if (evt.type == SDL_KEYDOWN && evt.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			mouse_captured = false;
			return true;
		}
		if (evt.type == SDL_MOUSEMOTION) {
			//Note: float(window_size.y) * camera->fovy is a pixels-to-radians conversion factor
			float yaw = evt.motion.xrel / float(window_size.y) * camera->fovy;
			float pitch = evt.motion.yrel / float(window_size.y) * camera->fovy;
			
			camera_yaw -= clamp(yaw, -50.f, 50.f);
			camera_pitch = clamp(camera_pitch - pitch, -1.5f, 1.5f);
			camera->transform->rotation = glm::normalize(
				//camera->transform->rotation
				glm::angleAxis(camera_yaw, glm::vec3(0.0f, 0.0f, 1.0f))
				* glm::angleAxis(camera_pitch, glm::vec3(1.0f, 0.0f, 0.0f))
				* glm::angleAxis(PI / 2.f, glm::vec3(1.0f, 0.0f, 0.0f))
			);
			//std::cout << "YAW: " << camera_yaw << std::endl;
			return true;
		}
	}
	return false;
}

void CratesMode::update(float elapsed) {
	glm::mat3 directions = glm::mat3_cast(
				glm::angleAxis(camera_yaw, glm::vec3(0.0f, 0.0f, 1.0f))
				* glm::angleAxis(PI / 2.f, glm::vec3(1.0f, 0.0f, 0.0f)));
	float amt = 3.f * elapsed;
	vec3 step = vec3(0,0,0);
	if (controls.right) step += amt * directions[0];
	if (controls.left) step -= amt * directions[0];
	if (controls.backward) step += amt * directions[2];
	if (controls.forward) step -= amt * directions[2];
	float len = length(step);
	if (len > amt) {
		step = step * amt / len;
	}

	walk_mesh.walk(walk_point, step);
	camera->transform->position = walk_mesh.world_point(walk_point) + vec3(0, 0, 0.5f);
	//std::cout << "CAM: " << camera->transform->position.x << ", " <<
	//	camera->transform->position.y << ", " << camera->transform->position.z << std::endl;

	{ //set sound positions:
		glm::mat4 cam_to_world = camera->transform->make_local_to_world();
		Sound::listener.set_position( cam_to_world[3] );
		//camera looks down -z, so right is +x:
		Sound::listener.set_right( glm::normalize(cam_to_world[0]) );

		if (loop) {
			//glm::mat4 large_crate_to_world = large_crate->transform->make_local_to_world();
			//loop->set_position( large_crate_to_world * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f) );
		}
	}

	dot_countdown -= elapsed;
	if (dot_countdown <= 0.0f) {
		dot_countdown = (rand() / float(RAND_MAX) * 2.0f) + 0.5f;
		//lm::mat4x3 small_crate_to_world = small_crate->transform->make_local_to_world();
		//sample_dot->play( small_crate_to_world * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f) );
	}

	for (Enemy &enemy : enemies) {
		if (enemy.update(elapsed, camera->transform->position, random_gen)) {
			std::cout << "LOSE" << std::endl;
		}
	}

	for (auto &button : buttons) {
		if (button == NULL) {
			continue;
		}
		vec3 dif = button->transform->parent->position + vec3(0,0,0.5f) - camera->transform->position;
		if (dot(dif, dif) < 0.5f) {
			std::cout << "DELETING BUTTON: " << button << std::endl;
			scene.delete_object(button);
			button = NULL;
		}
	}
}

void CratesMode::draw(glm::uvec2 const &drawable_size) {
	//set up basic OpenGL state:
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//set up light position + color:
	glUseProgram(vertex_color_program->program);
	glUniform3fv(vertex_color_program->sun_color_vec3, 1, glm::value_ptr(glm::vec3(0.81f, 0.81f, 0.76f)));
	glUniform3fv(vertex_color_program->sun_direction_vec3, 1, glm::value_ptr(glm::normalize(glm::vec3(-0.2f, 0.2f, 1.0f))));
	glUniform3fv(vertex_color_program->sky_color_vec3, 1, glm::value_ptr(glm::vec3(0.4f, 0.4f, 0.45f)));
	glUniform3fv(vertex_color_program->sky_direction_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 1.0f, 0.0f)));
	glUseProgram(0);

	//fix aspect ratio of camera
	camera->aspect = drawable_size.x / float(drawable_size.y);

	scene.draw(camera);

	if (Mode::current.get() == this) {
		glDisable(GL_DEPTH_TEST);
		std::string message;
		if (mouse_captured) {
			message = "ESCAPE TO UNGRAB MOUSE * WASD MOVE";
		} else {
			message = "CLICK TO GRAB MOUSE * ESCAPE QUIT";
		}
		float height = 0.06f;
		float width = text_width(message, height);
		draw_text(message, glm::vec2(-0.5f * width,-0.99f), height, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f));
		draw_text(message, glm::vec2(-0.5f * width,-1.0f), height, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

		glUseProgram(0);
	}

	GL_ERRORS();
}


void CratesMode::show_pause_menu() {
	std::shared_ptr< MenuMode > menu = std::make_shared< MenuMode >();

	std::shared_ptr< Mode > game = shared_from_this();
	menu->background = game;

	menu->choices.emplace_back("PAUSED");
	menu->choices.emplace_back("RESUME", [game](){
		Mode::set_current(game);
	});
	menu->choices.emplace_back("QUIT", [](){
		Mode::set_current(nullptr);
	});

	menu->selected = 1;

	Mode::set_current(menu);
}
