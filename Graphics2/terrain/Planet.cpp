#include "Planet.h"
#include <random>
#include "../renderer/glm/gtc/matrix_transform.hpp"

#include <iostream>

//Enums for the face of the planet
#define FACE_POS_X 0
#define FACE_NEG_X 1
#define FACE_POS_Y 2
#define FACE_NEG_Y 3
#define FACE_POS_Z 4
#define FACE_NEG_Z 5


#define TEX_REPEAT 64.0f

Planet::Planet() {
	lastPos = glm::vec3(0.0f, 0.0f, 0.0f);
}


Planet::~Planet() {
}


void Planet::generateTerrain(int octDepth) {
	//Allocate memory for heightmap
	std::cout << "Allocating memory for heightmap" << std::endl;
	for (unsigned int f = 0; f < 6; f++) {
		//For each face
		std::vector<std::vector<float>> face;
		for (unsigned int x = 0; x < numNodes; x++) {
			//For each x value
			std::vector<float> xVec;
			for (unsigned int y = 0; y < numNodes; y++) {
				xVec.push_back(0.0f);
			}
			face.push_back(xVec);
		}
		heightmap.push_back(face);
	}
	//Generate the heightmap
	diamondSquare();
	//Set up transformations for nodes
	createTransformations();

	/*
	For each level of detail calculate necessary size of grid (per face)
	Create mesh for each grid on each face at each LOD
	LOD0 = Every vertex (lots of grids needed to keep under 65k indices)
	LOD1 = Every other vertex
	LOD2 = Every 4th vertex
	Etc
	*/
	numGrids = numNodes / MAX_VERTS;
	for (int l = 0; l < NUM_LOD; l++) {
		std::cout << "Converting heightmap to meshes (LOD" << l << ")" << std::endl;
		//For each face
		for (int face = 0; face < 6; face++) {
			//For each grid cell
			for (int gridX = 0; gridX < numGrids; gridX++) {
				int minX = MAX_VERTS * gridX;
				int maxX = gridX == numGrids - 1 ? numNodes - 1 : minX + MAX_VERTS;
				for (int gridY = 0; gridY < numGrids; gridY++) {
					//Calculate bounds of the grid
					int minY = MAX_VERTS * gridY;
					int maxY = gridY == numGrids - 1 ? numNodes - 1 : minY + MAX_VERTS;
					generateGrid(l, face, minX, minY, maxX, maxY);
					makeMeshes(l, face, gridX, gridY);
				}
			}
		}
	}
	//*
	//Because for some reason this can take upwards of 10 mins to run in 1 thread, I'll just max out the CPU
	std::cout << "Generating collision data" << std::endl;
	for (int face = 0; face < 6; face++) {
		//For each grid cell
		for (int gridX = 0; gridX < numGrids; gridX++) {
			for (int gridY = 0; gridY < numGrids; gridY++) {
				PlanetMeshes* m = &LODS[0][face][gridX][gridY];
				if (m->grass) {
					std::thread* t = new std::thread([m, octDepth] { m->grass->createOctree(octDepth); });
				}
				if (m->sea) {
					std::thread* t = new std::thread([m, octDepth] { m->sea->createOctree(octDepth); });
				}
				if (m->rock) {
					std::thread* t = new std::thread([m, octDepth] { m->rock->createOctree(octDepth); });
				}
			}
		}
	}
	for (std::thread* t : threads) {
		t->join();
		delete t;
	}
	threads.clear();
	//*/
}

void Planet::updateVisible(SceneObject* highLod, SceneObject* lowLod, glm::vec3 pos, std::unordered_set<Mesh*> &highPoly) {
	//Determine grid position of pos
	glm::vec3 unitPos = glm::normalize(pos);
	float cubeScaling = 1.0f / glm::max(glm::max(abs(unitPos.x), abs(unitPos.y)), abs(unitPos.z));
	glm::vec3 cubeMapping = cubeScaling * unitPos;
	//Determine face position is on
	int face;
	if (cubeMapping.x < -1.0f + 1e-5f) {
		face = FACE_NEG_X;
	} else if (cubeMapping.x > 1.0f - 1e-5f) {
		face = FACE_POS_X;
	} else if (cubeMapping.y < -1.0f + 1e-5f) {
		face = FACE_NEG_Y;
	} else if (cubeMapping.y > 1.0f - 1e-5f) {
		face = FACE_POS_Y;
	} else if (cubeMapping.z < -1.0f + 1e-5f) {
		face = FACE_NEG_Z;
	} else if (cubeMapping.z > 1.0f - 1e-5f) {
		face = FACE_POS_Z;
	}
	//Get axes needed to calculate grid position
	float xTrans;
	float yTrans;
	switch (face) {
	case FACE_NEG_X:
		xTrans = cubeMapping.y;
		yTrans = cubeMapping.z;
		break;
	case FACE_POS_X:
		xTrans = cubeMapping.y;
		yTrans = -cubeMapping.z;
		break;
	case FACE_NEG_Y:
		xTrans = cubeMapping.z;
		yTrans = cubeMapping.x;
		break;
	case FACE_POS_Y:
		xTrans = -cubeMapping.z;
		yTrans = cubeMapping.x;
		break;
	case FACE_NEG_Z:
		xTrans = cubeMapping.y;
		yTrans = -cubeMapping.x;
		break;
	case FACE_POS_Z:
		xTrans = cubeMapping.y;
		yTrans = cubeMapping.x;
		break;
	default:
		xTrans = 0.0f;
		yTrans = 0.0f;
	}
	//Get grid position from pos
	int gridX = glm::clamp(static_cast<int>(floor((numGrids - 1) * (xTrans + 1.0f) / 2.0f)), 0, numGrids - 1);
	int gridY = glm::clamp(static_cast<int>(floor((numGrids - 1) * (yTrans + 1.0f) / 2.0f)), 0, numGrids - 1);
	float height = glm::dot(pos, pos);
	int startLod = NUM_LOD - 1;
	while (startLod > 1 && height < LOD_Distances[startLod]) { startLod--; }
	for (int f = 0; f < 6; f++) {
		for (int x = 0; x < numGrids; x++) {
			for (int y = 0; y < numGrids; y++) {
				int lod = startLod;
				//Get centre node
				int dX = MAX_VERTS;
				int dY = MAX_VERTS;
				if (x == numGrids - 1) {
					dX = numNodes - x * MAX_VERTS;
				}
				if (y == numGrids - 1) {
					dY = numNodes - y * MAX_VERTS;
				}
				int cX = x * MAX_VERTS + dX / 2;
				int cY = y * MAX_VERTS + dY / 2;
				glm::vec3 centre = getVertex(cX, cY, f, heightSea);
				//Cull back facing grids
				if (glm::dot(centre, pos) < 0.0f) {
					lod = -1;
				}
				//Change in LOD found
				changeLod(f, x, y, lod, highLod, lowLod, highPoly);
			}
		}
	}
	//Make closest grids high res if near enough
	if (height < LOD_Distances[0]) {
		for (int gx = -1; gx < 2; gx++) {
			for (int gy = -1; gy < 2; gy++) {
				int grX = gridX + gx;
				int grY = gridY + gy;
				int grF = face;
				moveInBoundsGrid(grF, grX, grY);
				changeLod(grF, grX, grY, 0, highLod, lowLod, highPoly);
			}
		}
	}
}

void Planet::hide() {
	std::unordered_set<Mesh*> hp;
	for (int f = 0; f < 6; f++) {
		for (int x = 0; x < numGrids; x++) {
			for (int y = 0; y < numGrids; y++) {
				changeLod(f, x, y, -1, NULL, NULL, hp);
			}
		}
	}
}

void inline Planet::changeLod(int f, int x, int y, int lod, SceneObject* highLod, SceneObject* lowLod, std::unordered_set<Mesh*> &highPoly) {
	if (lastLOD[f][x][y] != lod) {
		PlanetMeshes m;
		SceneObject* s = NULL;
		//Hide old meshes
		if (lastLOD[f][x][y] >= 0) {
			m = LODS[lastLOD[f][x][y]][f][x][y];
			if (m.grass) {
				m.grass->setParent(s);
			}
			if (m.sea) {
				m.sea->setParent(s);
			}
			if (m.rock) {
				m.rock->setParent(s);
			}
			if (lastLOD[f][x][y] == 0) {
				if (m.grass) {
					highPoly.erase(m.grass);
				}
				if (m.sea) {
					highPoly.erase(m.sea);
				}
				if (m.rock) {
					highPoly.erase(m.rock);
				}
			}
		}
		//Show new meshes
		if (lod >= 0) {
			m = LODS[lod][f][x][y];
			if (lod == 0) {
				s = highLod;
			} else if (lod > 0) {
				s = lowLod;
			}
			if (m.grass) {
				m.grass->setParent(s);
			}
			if (m.sea) {
				m.sea->setParent(s);
			}
			if (m.rock) {
				m.rock->setParent(s);
			}
			if (lod == 0) {
				if (m.grass) {
					highPoly.insert(m.grass);
				}
				if (m.sea) {
					highPoly.insert(m.sea);
				}
				if (m.rock) {
					highPoly.insert(m.rock);
				}
			}
		}
		//Update last LOD
		lastLOD[f][x][y] = lod;
	}
}

void Planet::setLODS(float lods[NUM_LOD]) {
	//Copy values (square for cheaper distance checks)
	//No error checks, because its your own damn fault if it breaks
	for (int i = 0; i < NUM_LOD; i++) {
		float l = lods[i] + planetScale;
		LOD_Distances[i] = l * l;
	}

}


void Planet::diamondSquare() {
	std::uniform_real_distribution<float> uni(minHeight, maxHeight);
	std::default_random_engine rng(seed);
	std::cout << "Creating corner nodes" << std::endl;
	//Create corners
	setNode(static_cast<float>(uni(rng)), FACE_NEG_Z, 0, 0);
	setNode(static_cast<float>(uni(rng)), FACE_NEG_Z, 0, numNodes - 1);
	setNode(static_cast<float>(uni(rng)), FACE_NEG_Z, numNodes - 1, 0);
	setNode(static_cast<float>(uni(rng)), FACE_NEG_Z, numNodes - 1, numNodes - 1);
	setNode(static_cast<float>(uni(rng)), FACE_POS_Z, 0, 0);
	setNode(static_cast<float>(uni(rng)), FACE_POS_Z, 0, numNodes - 1);
	setNode(static_cast<float>(uni(rng)), FACE_POS_Z, numNodes - 1, 0);
	setNode(static_cast<float>(uni(rng)), FACE_POS_Z, numNodes - 1, numNodes - 1);
	std::cout << "Applying diamond square algorithm" << std::endl;
	//Iteratively apply DSA
	int size = numNodes - 1;
	float rand_var = (maxHeight - minHeight) / 2;
	while (size > 1) {
		std::uniform_real_distribution<float> u(-rand_var, rand_var);
		//Diamond stage
		for (int f = 0; f < 6; f++) {
			for (int x = 0; x < (numNodes - 1); x += size) {
				for (int y = 0; y < (numNodes - 1); y += size) {
					//Get height of surrounding nodes
					float p1, p2, p3, p4;
					p1 = getNode(f, x, y);
					p2 = getNode(f, x, y + size);
					p3 = getNode(f, x + size, y + size);
					p4 = getNode(f, x + size, y);
					setNode((p1 + p2 + p3 + p4) / 4.0f + u(rng), f, x + size / 2, y + size / 2);
				}
			}
		}
		//Square stages
		for (int f = 0; f < 6; f++) {
			//(x + y - 1) � y
			int s = (numNodes - 1 + size / 2) / (size / 2);
			for (int x = 0; x < s; x++) {
				for (int z = 0; z < s; z++) {
					if (x % 2 == z % 2) {
						continue;
					}
					int px = x * (size / 2);
					int pz = z * (size / 2);
					float h = 0;
					h += getNode(f, px - (size / 2), pz);
					h += getNode(f, px + (size / 2), pz);
					h += getNode(f, px, pz - (size / 2));
					h += getNode(f, px, pz + (size / 2));
					h /= 4;
					setNode(h + u(rng), f, px, pz);
				}
			}
		}
		//Decrease size
		rand_var *= static_cast<float>(pow(2, -roughness));
		size = size / 2;
	}
}

inline void Planet::createTransformations() {
	//Transformations to apply to each face
	//Centre planet on 0,0,0 (model space)
	float halfNodes = static_cast<float>(numNodes - 1) / 2.0f;
	glm::mat4 pos = glm::translate(glm::mat4(1), glm::vec3(-halfNodes, halfNodes, -halfNodes));

	faceTrans[FACE_POS_X] = glm::rotate(glm::mat4(1), -glm::half_pi<float>(), glm::vec3(0.0f, 0.0f, 1.0f)) * glm::rotate(glm::mat4(1), glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f)) * pos;
	faceTrans[FACE_NEG_X] = glm::rotate(glm::mat4(1), glm::half_pi<float>(), glm::vec3(0.0f, 0.0f, 1.0f)) * pos;
	faceTrans[FACE_POS_Y] = glm::rotate(glm::mat4(1), glm::half_pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f)) * pos;
	faceTrans[FACE_NEG_Y] = glm::rotate(glm::mat4(1), glm::pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f)) * glm::rotate(glm::mat4(1), glm::half_pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f)) * pos;
	faceTrans[FACE_POS_Z] = glm::rotate(glm::mat4(1), glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f)) * glm::rotate(glm::mat4(1), glm::half_pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f)) * pos;
	faceTrans[FACE_NEG_Z] = glm::rotate(glm::mat4(1), -glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f)) * glm::rotate(glm::mat4(1), -glm::half_pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f)) * pos;
}

inline void Planet::generateGrid(int l, int f, int minX, int minY, int maxX, int maxY) {
	int step = 1 << l;
	int nodesX = (maxX - minX) / step;
	int nodesY = (maxY - minY) / step;
	float scale = l == 0 ? 1.0f : lowLodScale;
	nodesInGrid = nodesX;
	//Generate arrays for vertex data
	for (int y = 0; y <= nodesY; y++) {
		int lY = y * step + minY;
		for (int x = 0; x <= nodesX; x++) {
			int lX = x * step + minX;
			vert_sea.push_back(getVertex(lX, lY, f, heightSea) * scale);
			glm::vec3 v = getVertex(lX, lY, f);
			vert_land.push_back(v * scale);
			uv.push_back(TEX_REPEAT * glm::vec2(static_cast<float>(lX) / (numNodes), static_cast<float>(lY) / (numNodes)));
			norm.push_back(glm::normalize(v));
		}
	}
	for (int y = 0; y <= nodesY; y++) {
		int lY = y * step + minY;
		for (int x = y % 2; x <= nodesX; x += 2) {
			//Transform (x,y) to correct LOD
			int lX = x * step + minX;
			int xs[6];
			int ys[6];
			xs[0] = lX;
			ys[0] = lY;
			xs[3] = x;
			ys[3] = y;
			if (x > 0) {
				if (y > 0) {
					//Negative Y
					xs[1] = lX;
					ys[1] = lY - step;
					xs[4] = x;
					ys[4] = y - 1;
					//Negative X
					xs[2] = lX - step;
					ys[2] = lY;
					xs[5] = x - 1;
					ys[5] = y;
					addTriangle(l, f, xs, ys);
				}
				if (y < nodesY) {
					//Negative X
					xs[1] = lX - step;
					ys[1] = lY;
					xs[4] = x - 1;
					ys[4] = y;
					//Positive Y
					xs[2] = lX;
					ys[2] = lY + step;
					xs[5] = x;
					ys[5] = y + 1;
					addTriangle(l, f, xs, ys);
				}
			}
			if (x < nodesX) {
				//Neighbours to the -y
				if (y > 0) {
					//Positive X
					xs[1] = lX + step;
					ys[1] = lY;
					xs[4] = x + 1;
					ys[4] = y;
					//Negative Y
					xs[2] = lX;
					ys[2] = lY - step;
					xs[5] = x;
					ys[5] = y - 1;
					addTriangle(l, f, xs, ys);
				}
				//Neighbours to the +y
				if (y < nodesY) {
					//Positive Y
					xs[1] = lX;
					ys[1] = lY + step;
					xs[4] = x;
					ys[4] = y + 1;
					//Positive X
					xs[2] = lX + step;
					ys[2] = lY;
					xs[5] = x + 1;
					ys[5] = y;
					addTriangle(l, f, xs, ys);
				}
			}
		}
	}
}

inline void Planet::makeMeshes(int l, int face, int gridX, int gridY) {
	PlanetMeshes meshes;
	if (LODS.size() <= static_cast<unsigned int>(l)) {
		std::vector<std::vector<std::vector<PlanetMeshes>>> n;
		LODS.push_back(n);
	}
	if (LODS[l].size() <= static_cast<unsigned int>(face)) {
		std::vector<std::vector<PlanetMeshes>> n;
		LODS[l].push_back(n);
	}
	if (LODS[l][face].size() <= static_cast<unsigned int>(gridX)) {
		std::vector<PlanetMeshes> n;
		LODS[l][face].push_back(n);
	}
	if (LODS[l][face][gridX].size() <= gridY) {
		LODS[l][face][gridX].push_back(meshes);
	}
	if (lastLOD.size() <= static_cast<unsigned int>(face)) {
		std::vector<std::vector<int>> last;
		lastLOD.push_back(last);
	}
	if (lastLOD[face].size() <= static_cast<unsigned int>(gridX)) {
		std::vector<int> last;
		lastLOD[face].push_back(last);
	}
	if (lastLOD[face][gridX].size() <= static_cast<unsigned int>(gridY)) {
		lastLOD[face][gridX].push_back(-1);
	}
	//Set mesh
	if (ind_sea.size() > 0) {
		Mesh* m = new Mesh();
		m->setMesh(ind_sea, vert_sea, uv, norm, std::vector<glm::vec3>(), std::vector<glm::vec3>());
		m->useNormalTexture = false;
		meshes.sea = m;
		meshes.sea->setDiffuse(seaTex);
		meshes.sea->setShininess(32.0f);
		meshes.sea->setSpecular(seaSpec);
	} else {
		meshes.sea = NULL;
	}
	//Set mesh
	if (ind_land.size() > 0) {
		Mesh* m = new Mesh();
		m->setMesh(ind_land, vert_land, uv, norm, std::vector<glm::vec3>(), std::vector<glm::vec3>());
		m->useNormalTexture = false;
		meshes.grass = m;
		meshes.grass->setDiffuse(landTex);
		meshes.grass->setShininess(32.0f);
		meshes.grass->setSpecular(landSpec);
	} else {
		meshes.grass = NULL;
	}
	//Set mesh
	if (ind_rock.size() > 0) {
		Mesh* m = new Mesh();
		m->setMesh(ind_rock, vert_land, uv, norm, std::vector<glm::vec3>(), std::vector<glm::vec3>());
		m->useNormalTexture = false;
		meshes.rock = m;
		meshes.rock->setDiffuse(rockTex);
		meshes.rock->setShininess(32.0f);
		meshes.rock->setSpecular(rockSpec);
	} else {
		meshes.rock = NULL;
	}
	ind_sea.clear();
	ind_land.clear();
	ind_rock.clear();
	vert_land.clear();
	vert_sea.clear();
	uv.clear();
	norm.clear();
	LODS[l][face][gridX][gridY] = meshes;
}

void Planet::setNode(float value, unsigned int face, unsigned int x, unsigned int y) {
	//I think opengl's backwards z axis messed something up and this sort of fixes it
	heightmap[face][y][x] = value;
	if (face == FACE_POS_X) {
		//Left edge
		if (x == 0) {
			heightmap[FACE_POS_Z][y][numNodes - 1] = value;
		}
		//Right edge
		if (x == numNodes - 1) {
			heightmap[FACE_NEG_Z][y][0] = value;
		}
		//Bottom edge
		if (y == 0) {
			heightmap[FACE_NEG_Y][numNodes - 1 - x][numNodes - 1] = value;
		}
		//Top edge
		if (y == numNodes - 1) {
			heightmap[FACE_POS_Y][x][numNodes - 1] = value;
		}
	}
	if (face == FACE_NEG_X) {
		//Left edge
		if (x == 0) {
			heightmap[FACE_NEG_Z][y][numNodes - 1] = value;
		}
		//Right edge
		if (x == numNodes - 1) {
			heightmap[FACE_POS_Z][y][0] = value;
		}
		//Bottom edge
		if (y == 0) {
			heightmap[FACE_NEG_Y][x][0] = value;
		}
		//Top edge
		if (y == numNodes - 1) {
			heightmap[FACE_POS_Y][numNodes - 1 - x][0] = value;
		}
	}
	if (face == FACE_POS_Y) {
		//Left edge
		if (x == 0) {
			heightmap[FACE_NEG_X][numNodes - 1][numNodes - 1 - y] = value;
		}
		//Right edge
		if (x == numNodes - 1) {
			heightmap[FACE_POS_X][numNodes - 1][y] = value;
		}
		//Bottom edge
		if (y == 0) {
			heightmap[FACE_NEG_Z][numNodes - 1][x] = value;
		}
		//Top edge
		if (y == numNodes - 1) {
			heightmap[FACE_POS_Z][numNodes - 1][numNodes - 1 - x] = value;
		}
	}
	if (face == FACE_NEG_Y) {
		//Left edge
		if (x == 0) {
			heightmap[FACE_NEG_X][0][y] = value;
		}
		//Right edge
		if (x == numNodes - 1) {
			heightmap[FACE_POS_X][0][numNodes - 1 - y] = value;
		}
		//Bottom edge
		if (y == 0) {
			heightmap[FACE_POS_Z][0][x] = value;
		}
		//Top edge
		if (y == numNodes - 1) {
			heightmap[FACE_NEG_Z][0][x] = value;
		}
	}
	if (face == FACE_POS_Z) {
		//Left edge
		if (x == 0) {
			heightmap[FACE_NEG_X][y][numNodes - 1] = value;
		}
		//Right edge
		if (x == numNodes - 1) {
			heightmap[FACE_POS_X][y][0] = value;
		}
		//Bottom edge
		if (y == 0) {
			heightmap[FACE_NEG_Y][numNodes - 1][x] = value;
		}
		//Top edge
		if (y == numNodes - 1) {
			heightmap[FACE_POS_Y][0][x] = value;
		}
	}
	if (face == FACE_NEG_Z) {
		//Left edge
		if (x == 0) {
			heightmap[FACE_POS_X][y][numNodes - 1] = value;
		}
		//Right edge
		if (x == numNodes - 1) {
			heightmap[FACE_NEG_X][y][0] = value;
		}
		//Bottom edge
		if (y == 0) {
			heightmap[FACE_NEG_Y][0][numNodes - 1 - x] = value;
		}
		//Top edge
		if (y == numNodes - 1) {
			heightmap[FACE_POS_Y][numNodes - 1][numNodes - 1 - x] = value;
		}
	}
}

void Planet::moveInBounds(int & face, int & x, int & y) {
	//Handle wrapping (mostly, don't expect large numbers to work)
	if (face == FACE_POS_X) {
		if (x < 0) {
			face = FACE_POS_Z;
			x += numNodes - 1;
		} else if (x > numNodes - 1) {
			face = FACE_NEG_Z;
			x -= numNodes - 1;
		} else if (y < 0) {
			face = FACE_NEG_Y;
			int tmp = numNodes - 1 + y;
			y = numNodes - 1 - x;
			x = tmp;
		} else if (y > numNodes - 1) {
			face = FACE_POS_Y;
			int tmp = x;
			x = numNodes - 1 - (y - (numNodes - 1));
			y = tmp;
		}
	} else if (face == FACE_NEG_X) {
		if (x < 0) {
			face = FACE_NEG_Z;
			x += numNodes - 1;
		} else if (x > numNodes - 1) {
			face = FACE_POS_Z;
			x -= numNodes - 1;
		} else if (y < 0) {
			face = FACE_NEG_Y;
			int tmp = -y;
			y = x;
			x = tmp;
		} else if (y > numNodes - 1) {
			face = FACE_POS_Y;
			int tmp = (numNodes - 1) - x;
			x = y - (numNodes - 1);
			y = tmp;
		}
	} else if (face == FACE_POS_Y) {
		if (x < 0) {
			face = FACE_NEG_X;
			int tmp = (numNodes - 1) + x;
			x = (numNodes - 1) - y;
			y = tmp;
		} else if (x > numNodes - 1) {
			face = FACE_POS_X;
			int tmp = numNodes - 1 - (x - (numNodes - 1));
			x = y;
			y = tmp;
		} else if (y < 0) {
			face = FACE_POS_Z;
			y += numNodes - 1;
		} else if (y > numNodes - 1) {
			face = FACE_NEG_Z;
			x = numNodes - 1 - x;
			y = numNodes - 1 - (y - (numNodes - 1));
		}
	} else if (face == FACE_NEG_Y) {
		if (x < 0) {
			face = FACE_NEG_X;
			int tmp = -x;
			x = y;
			y = tmp;
		} else if (x > numNodes - 1) {
			face = FACE_POS_X;
			int tmp = x - (numNodes - 1);
			x = numNodes - 1 - y;
			y = tmp;
		} else if (y < 0) {
			face = FACE_NEG_Z;
			x = numNodes - 1 - x;
			y = -y;
		} else if (y > numNodes - 1) {
			face = FACE_NEG_Z;
			y -= numNodes - 1;
		}
	} else if (face == FACE_POS_Z) {
		if (x < 0) {
			face = FACE_NEG_X;
			x += numNodes - 1;
		} else if (x > numNodes - 1) {
			face = FACE_POS_X;
			x -= numNodes - 1;
		} else if (y < 0) {
			face = FACE_NEG_Y;
			y = numNodes - 1 + y;
		} else if (y > numNodes - 1) {
			face = FACE_POS_Y;
			y -= numNodes - 1;
		}
	} else if (face == FACE_NEG_Z) {
		if (x < 0) {
			face = FACE_POS_X;
			x += numNodes - 1;
		} else if (x > numNodes - 1) {
			face = FACE_NEG_X;
			x -= numNodes - 1;
		} else if (y < 0) {
			face = FACE_NEG_Y;
			x = numNodes - 1 - x;
			y = -y;
		} else if (y > numNodes - 1) {
			face = FACE_POS_Y;
			x = numNodes - 1 - x;
			y = numNodes - 1 - (y - (numNodes - 1));
		}
	}
	if (x >= heightmap[face].size() || x < 0 || y >= heightmap[face][x].size() || y < 0) {
		//Damn it go for another pass
		moveInBounds(face, x, y);
	}
}

void Planet::moveInBoundsGrid(int& face, int& x, int& y) {
	//Handle wrapping (mostly, don't expect large numbers to work)
	if (face == FACE_POS_X) {
		if (x < 0) {
			face = FACE_POS_Z;
			x += numGrids - 1;
		} else if (x > numGrids - 1) {
			face = FACE_NEG_Z;
			x -= numGrids - 1;
		} else if (y < 0) {
			face = FACE_NEG_Y;
			int tmp = numGrids - 1 + y;
			y = numGrids - 1 - x;
			x = tmp;
		} else if (y > numGrids - 1) {
			face = FACE_POS_Y;
			int tmp = x;
			x = numGrids - 1 - (y - (numGrids - 1));
			y = tmp;
		}
	} else if (face == FACE_NEG_X) {
		if (x < 0) {
			face = FACE_NEG_Z;
			x += numGrids - 1;
		} else if (x > numGrids - 1) {
			face = FACE_POS_Z;
			x -= numGrids - 1;
		} else if (y < 0) {
			face = FACE_NEG_Y;
			int tmp = -y;
			y = x;
			x = tmp;
		} else if (y > numGrids - 1) {
			face = FACE_POS_Y;
			int tmp = (numGrids - 1) - x;
			x = y - (numGrids - 1);
			y = tmp;
		}
	} else if (face == FACE_POS_Y) {
		if (x < 0) {
			face = FACE_NEG_X;
			int tmp = (numGrids - 1) + x;
			x = (numGrids - 1) - y;
			y = tmp;
		} else if (x > numGrids - 1) {
			face = FACE_POS_X;
			int tmp = numGrids - 1 - (x - (numGrids - 1));
			x = y;
			y = tmp;
		} else if (y < 0) {
			face = FACE_POS_Z;
			y += numGrids - 1;
		} else if (y > numGrids - 1) {
			face = FACE_NEG_Z;
			x = numGrids - 1 - x;
			y = numGrids - 1 - (y - (numGrids - 1));
		}
	} else if (face == FACE_NEG_Y) {
		if (x < 0) {
			face = FACE_NEG_X;
			int tmp = -x;
			x = y;
			y = tmp;
		} else if (x > numGrids - 1) {
			face = FACE_POS_X;
			int tmp = x - (numGrids - 1);
			x = numGrids - 1 - y;
			y = tmp;
		} else if (y < 0) {
			face = FACE_NEG_Z;
			x = numGrids - 1 - x;
			y = -y;
		} else if (y > numGrids - 1) {
			face = FACE_NEG_Z;
			y -= numGrids - 1;
		}
	} else if (face == FACE_POS_Z) {
		if (x < 0) {
			face = FACE_NEG_X;
			x += numGrids - 1;
		} else if (x > numGrids - 1) {
			face = FACE_POS_X;
			x -= numGrids - 1;
		} else if (y < 0) {
			face = FACE_NEG_Y;
			y = numGrids - 1 + y;
		} else if (y > numGrids - 1) {
			face = FACE_POS_Y;
			y -= numGrids - 1;
		}
	} else if (face == FACE_NEG_Z) {
		if (x < 0) {
			face = FACE_POS_X;
			x += numGrids - 1;
		} else if (x > numGrids - 1) {
			face = FACE_NEG_X;
			x -= numGrids - 1;
		} else if (y < 0) {
			face = FACE_NEG_Y;
			x = numGrids - 1 - x;
			y = -y;
		} else if (y > numGrids - 1) {
			face = FACE_POS_Y;
			x = numGrids - 1 - x;
			y = numGrids - 1 - (y - (numGrids - 1));
		}
	}
	if (x >= numGrids || x < 0 || y >= numGrids || y < 0) {
		//Damn it go for another pass
		moveInBoundsGrid(face, x, y);
	}
}

float Planet::getNode(int face, int x, int y) {
	moveInBounds(face, x, y);
	return heightmap[face][x][y];
}

glm::vec3 Planet::getVertex(int x, int y, int face) {
	//Get position on sphere
	glm::vec3 p = glm::vec3(faceTrans[face] * glm::vec4(x, 0.0f, y, 1.0f));
	p = glm::normalize(p);
	//Extrude by heightmap
	float height = 1.0f + getNode(face, x, y);
	p = p * height * planetScale;
	return p;
}

glm::vec3 Planet::getVertex(int x, int y, int face, float height) {
	//Get position on sphere
	glm::vec3 p = glm::vec3(faceTrans[face] * glm::vec4(x, 0.0f, y, 1.0f));
	p = glm::normalize(p);
	//Extrude by heightmap
	p = p * (height + 1.0f) * planetScale;
	return p;
}

void Planet::addTriangle(int l, int f, int(&xs)[6], int(&ys)[6]) {
	bool addSea = false;
	bool addLand = false;
	bool addRock = false;
	//Count how many vertices are land height(-) and how many are rock height(+)
	float averageHeight = 0;
	for (int i = 0; i < 3; i++) {
		float h = getNode(f, xs[i], ys[i]);
		if (h < heightSea) {
			addSea = true;
		} else {
			addLand = true;
		}
		averageHeight += h;
	}
	addRock = averageHeight / 3.0f > heightRock;
	//If any point is below sea level add all to sea (setting height to sea level)
	if (addSea) {
		for (int i = 0; i < 3; i++) {
			unsigned short pos = xs[i + 3] + (nodesInGrid + 1) * ys[i + 3];
			ind_sea.push_back(pos);
		}
	}
	//If any point is above sea level add to land
	if (addLand) {
		if (addRock) {
			for (int i = 0; i < 3; i++) {
				for (int i = 0; i < 3; i++) {
					unsigned short pos = xs[i + 3] + (nodesInGrid + 1) * ys[i + 3];
					ind_rock.push_back(pos);
				}
			}
		} else {
			for (int i = 0; i < 3; i++) {
				for (int i = 0; i < 3; i++) {
					unsigned short pos = xs[i + 3] + (nodesInGrid + 1) * ys[i + 3];
					ind_land.push_back(pos);
				}
			}
		}
	}
}

inline void Planet::changeParent(PlanetMeshes& meshes, SceneObject* parent) {
	if (meshes.sea) {
		meshes.sea->setParent(parent);
	}
	if (meshes.grass) {
		meshes.grass->setParent(parent);
	}
	if (meshes.rock) {
		meshes.rock->setParent(parent);
	}
}
