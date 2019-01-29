#include "Terrain.h"

#include "triangulation.h"
#include "HeightMap.h"
#include "PerlinNoise.h"
#include "Water.h"

#include <boglfw/renderOpenGL/Shape3D.h>
#include <boglfw/renderOpenGL/shader.h>
#include <boglfw/renderOpenGL/Viewport.h>
#include <boglfw/renderOpenGL/Camera.h>
#include <boglfw/renderOpenGL/TextureLoader.h>
#include <boglfw/math/math3D.h>
#include <boglfw/World.h>
#include <boglfw/utils/rand.h>
#include <boglfw/utils/log.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/type_ptr.hpp>

//#include <rp3d/reactphysics3d.h>

#include <GL/glew.h>

#include <new>
#include <algorithm>

// TODO: optimize texture mapping with atlas and texture array: https://www.khronos.org/opengl/wiki/Array_Texture

struct Terrain::TerrainVertex {
	static const unsigned nTextures = 5;
	
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec3 color;
	glm::vec2 uv[nTextures];	// uvs for each texture layer
	glm::vec4 texBlendFactor;	// 4 texture blend factors: x is between grass1 & grass2,
								// 							y between rock1 & rock2
								//							z between grass and rock
								//							w between everything and sand
	
	TerrainVertex() = default;
};

struct TextureInfo {
	unsigned texID = 0;		// GL texture ID
	float wWidth = 1.f;		// width of texture in world units (meters)
	float wHeight = 1.f;	// height/length of texture in world units (meters)

	~TextureInfo() {
		if (texID)
			glDeleteTextures(1, &texID);
	}
};

struct Terrain::RenderData {
	unsigned VAO_;
	unsigned VBO_;
	unsigned IBO_;
	unsigned shaderProgram_;
	unsigned iPos_;
	unsigned iNormal_;
	unsigned iColor_;
	unsigned iUV_;
	unsigned iTexBlendF_;
	unsigned imPV_;
	unsigned iSampler_;
	TextureInfo textures_[TerrainVertex::nTextures];
};

template<>
float nth_elem(Terrain::TerrainVertex const& v, unsigned n) {
	return	n==0 ? v.pos.x : 
			n==1 ? v.pos.z : 
			0.f;
}

Terrain::Terrain()
{
	renderData_ = new RenderData();
	renderData_->shaderProgram_ = Shaders::createProgram("data/shaders/terrain.vert", "data/shaders/terrain.frag");
	if (!renderData_->shaderProgram_) {
		ERROR("Failed to load terrain shaders!");
		throw std::runtime_error("Failed to load terrain shaders");
	}
	renderData_->iPos_ = glGetAttribLocation(renderData_->shaderProgram_, "pos");
	renderData_->iNormal_ = glGetAttribLocation(renderData_->shaderProgram_, "normal");
	renderData_->iColor_ = glGetAttribLocation(renderData_->shaderProgram_, "color");
	renderData_->iUV_ = glGetAttribLocation(renderData_->shaderProgram_, "uv");
	renderData_->iTexBlendF_ = glGetAttribLocation(renderData_->shaderProgram_, "texBlendFactor");
	renderData_->imPV_ = glGetUniformLocation(renderData_->shaderProgram_, "mPV");
	renderData_->iSampler_ = glGetUniformLocation(renderData_->shaderProgram_, "tex");
	glGenVertexArrays(1, &renderData_->VAO_);
	glGenBuffers(1, &renderData_->VBO_);
	glGenBuffers(1, &renderData_->IBO_);
	
	glBindVertexArray(renderData_->VAO_);
	glBindBuffer(GL_ARRAY_BUFFER, renderData_->VBO_);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderData_->IBO_);
	glEnableVertexAttribArray(renderData_->iPos_);
	glVertexAttribPointer(renderData_->iPos_, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex),
		(void*)offsetof(TerrainVertex, pos));
	if (renderData_->iNormal_ > 0) {
		glEnableVertexAttribArray(renderData_->iNormal_);
		glVertexAttribPointer(renderData_->iNormal_, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex),
			(void*)offsetof(TerrainVertex, normal));
	}
	if (renderData_->iColor_ > 0) {
		glEnableVertexAttribArray(renderData_->iColor_);
		glVertexAttribPointer(renderData_->iColor_, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex),
			(void*)offsetof(TerrainVertex, color));
	}
	if (renderData_->iUV_ > 0) {
		for (unsigned i=0; i<TerrainVertex::nTextures; i++) {
			glEnableVertexAttribArray(renderData_->iUV_ + i);
			glVertexAttribPointer(renderData_->iUV_ + i, 2, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), 
				(void*)(offsetof(TerrainVertex, uv) + i*sizeof(TerrainVertex::uv[0])));
		}
	}
	if (renderData_->iTexBlendF_ > 0) {
		glEnableVertexAttribArray(renderData_->iTexBlendF_);
		glVertexAttribPointer(renderData_->iTexBlendF_, 4, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex),
			(void*)offsetof(TerrainVertex, texBlendFactor));
	}
	glBindVertexArray(0);

	loadTextures();
	
	pWater_ = new Water();
}

Terrain::~Terrain()
{
	clear();
	delete renderData_, renderData_ = nullptr;
	delete pWater_, pWater_ = nullptr;
}

void Terrain::clear() {
	if (pVertices_)
		free(pVertices_), pVertices_ = nullptr, nVertices_ = 0;
	triangles_.clear();
}

void Terrain::loadTextures() {
	renderData_->textures_[0].texID = TextureLoader::loadFromPNG("data/textures/terrain/dirt3.png", true);
	glBindTexture(GL_TEXTURE_2D, renderData_->textures_[0].texID);
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	renderData_->textures_[0].wWidth = 2.f;
	renderData_->textures_[0].wHeight = 2.f;

	renderData_->textures_[1].texID = TextureLoader::loadFromPNG("data/textures/terrain/grass1.png", true);
	glBindTexture(GL_TEXTURE_2D, renderData_->textures_[1].texID);
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	renderData_->textures_[1].wWidth = 3.f;
	renderData_->textures_[1].wHeight = 3.f;
	
	renderData_->textures_[2].texID = TextureLoader::loadFromPNG("data/textures/terrain/rock1.png", true);
	glBindTexture(GL_TEXTURE_2D, renderData_->textures_[2].texID);
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	renderData_->textures_[2].wWidth = 3.f;
	renderData_->textures_[2].wHeight = 3.f;

	renderData_->textures_[3].texID = TextureLoader::loadFromPNG("data/textures/terrain/rock3.png", true);
	glBindTexture(GL_TEXTURE_2D, renderData_->textures_[3].texID);
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	renderData_->textures_[3].wWidth = 4.f;
	renderData_->textures_[3].wHeight = 4.f;
	
	renderData_->textures_[4].texID = TextureLoader::loadFromPNG("data/textures/terrain/sand1.png", true);
	glBindTexture(GL_TEXTURE_2D, renderData_->textures_[4].texID);
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	renderData_->textures_[4].wWidth = 4.f;
	renderData_->textures_[4].wHeight = 4.f;

	glBindTexture(GL_TEXTURE_2D, 0);
}

void validateSettings(TerrainSettings const& s) {
	assert(s.width > 0);
	assert(s.length > 0);
	assert(s.maxElevation > s.minElevation);
	assert(s.vertexDensity > 0);
	assert(s.width >= 1.f / s.vertexDensity);
	assert(s.length >= 1.f / s.vertexDensity);
}

void Terrain::generate(TerrainSettings const& settings) {
	validateSettings(settings);
	clear();
	settings_ = settings;

	rows_ = (unsigned)ceil(settings_.length * settings_.vertexDensity) + 1;
	cols_ = (unsigned)ceil(settings_.width * settings_.vertexDensity) + 1;
	
	// we need to generate some 'skirt' vertices that will encompass the entire terrain in a circle, 
	// in order to extend the sea-bed away from the main terrain
	float terrainRadius = sqrtf(settings_.width * settings_.width + settings_.length * settings_.length) * 0.5f;
	float seaBedRadius = terrainRadius * 2.5f;
	float skirtVertSpacing = 30.f; // meters
	unsigned nSkirtVerts = (2 * PI * seaBedRadius) / skirtVertSpacing;
	float skirtVertSector = 2 * PI / nSkirtVerts; // sector size between two skirt vertices
	nVertices_ = rows_ * cols_ + nSkirtVerts;
	pVertices_ = (TerrainVertex*)malloc(sizeof(TerrainVertex) * nVertices_);
	
	glm::vec3 topleft {-settings_.width * 0.5f, 0.f, -settings.length * 0.5f};
	float dx = settings_.width / (cols_ - 1);
	float dz = settings_.length / (rows_ - 1);
	gridSpacing_ = {dx, dz};
	// compute terrain vertices
	for (unsigned i=0; i<rows_; i++)
		for (unsigned j=0; j<cols_; j++) {
			glm::vec2 jitter { randf() * settings_.relativeRandomJitter * dx, randf() * settings_.relativeRandomJitter * dz };
			new(&pVertices_[i*cols_ + j]) TerrainVertex {
				topleft + glm::vec3(dx * j + jitter.x, settings_.minElevation, dz * i + jitter.y),	// position
				{0.f, 1.f, 0.f},																	// normal
				{1.f, 1.f, 1.f},																	// color
				{{0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}},						// uvs
				{0.f, 0.f, 0.f, 0.f}																// tex blend factor
			};
			// compute UVs
			for (unsigned t=0; t<TerrainVertex::nTextures; t++) {
				pVertices_[i*cols_ + j].uv[t].x = (pVertices_[i*cols_ + j].pos.x - topleft.x) / renderData_->textures_[t].wWidth;
				pVertices_[i*cols_ + j].uv[t].y = (pVertices_[i*cols_ + j].pos.z - topleft.z) / renderData_->textures_[t].wHeight;
			}
		}
	// compute skirt vertices
	for (unsigned i=0; i<nSkirtVerts; i++) {
		float x = seaBedRadius * cosf(i*skirtVertSector);
		float z = seaBedRadius * sinf(i*skirtVertSector);
		new(&pVertices_[rows_*cols_+i]) TerrainVertex {
			{ x, settings_.minElevation, z },								// position
			{ 0.f, 1.f, 0.f },												// normal
			{ 1.f, 1.f, 1.f },												// color
			{ {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f} },	// uvs
			{ 0.f, 0.f, 0.f, 1.f }											// tex blend factor
		};
		// compute UVs
		for (unsigned t=0; t<TerrainVertex::nTextures; t++) {
			pVertices_[rows_*cols_ + i].uv[t].x = (x - topleft.x) / renderData_->textures_[t].wWidth;
			pVertices_[rows_*cols_ + i].uv[t].y = (z - topleft.z) / renderData_->textures_[t].wHeight;
		}
	}
	
	int trRes = triangulate(pVertices_, nVertices_, triangles_);
	if (trRes < 0) {
		ERROR("Failed to triangulate terrain mesh!");
		return;
	}
	fixTriangleWinding();	// after triangulation some triangles are ccw, we need to fix them

	computeDisplacements();
	computeNormals();
	computeTextureWeights();
	
	updateRenderBuffers();
	updatePhysics();
	
	pWater_->generate(WaterParams {
		settings_.seaLevel,				// water level
		terrainRadius,					// inner radius
		seaBedRadius - terrainRadius,	// outer extent
		0.05f,							// vertex density
		false							// constrain to circle
	});
}

void Terrain::fixTriangleWinding() {
	// all triangles must be CW as seen from above
	for (auto &t : triangles_) {
		glm::vec3 n = glm::cross(pVertices_[t.iV2].pos - pVertices_[t.iV1].pos, pVertices_[t.iV3].pos - pVertices_[t.iV1].pos);
		if (n.y < 0) {
			// triangle is CCW, we need to reverse it
			xchg(t.iV1, t.iV3);	// exchange vertices 1 and 3
			xchg(t.iN12, t.iN23); // exchange edges 1-2 and 2-3
		}
	}
}

void Terrain::computeDisplacements() {
	HeightmapParams hparam;
	hparam.width = settings_.width / 8;
	hparam.length = settings_.length / 8;
	hparam.minHeight = settings_.minElevation;
	hparam.maxHeight = settings_.maxElevation;
	HeightMap height(hparam);
	height.meltEdges(5);
	PerlinNoise pnoise(settings_.width, settings_.length);

	glm::vec3 topleft {-settings_.width * 0.5f, 0.f, -settings_.length * 0.5f};
	for (unsigned i=1; i<rows_-1; i++) // we leave the edge vertices at zero to avoid artifacts with the skirt
		for (unsigned j=1; j<cols_-1; j++) {
			unsigned k = i*cols_ + j;
			float u = (pVertices_[k].pos.x - topleft.x) / settings_.width;
			float v = (pVertices_[k].pos.z - topleft.z) / settings_.length;

			float perlinAmp = (settings_.maxElevation - settings_.minElevation) * 0.1f;
			float perlin = pnoise.get(u/8, v/8, 1.f) * perlinAmp * 0.3
							+ pnoise.get(u/4, v/4, 1.f) * perlinAmp * 0.2
							+ pnoise.get(u/2, v/2, 1.f) * perlinAmp * 0.1
							+ pnoise.get(u/1, v/1, 1.f) * perlinAmp * 0.05;

			pVertices_[k].pos.y = height.value(u, v) * settings_.bigRoughness
									+ perlin * settings_.smallRoughness;
			
			// TODO : use vertex colors with perlin noise for more variety

			// debug
			//float hr = (pVertices_[k].pos.y - settings_.minElevation) / (settings_.maxElevation - settings_.minElevation);
			//pVertices_[k].color = {1.f - hr, hr, 0};
		}
}

void Terrain::computeNormals() {
	for (auto &t : triangles_) {
		glm::vec3 n = glm::cross(pVertices_[t.iV2].pos - pVertices_[t.iV1].pos, pVertices_[t.iV3].pos - pVertices_[t.iV1].pos);
		n = glm::normalize(n);
		pVertices_[t.iV1].normal += n;
		pVertices_[t.iV2].normal += n;
		pVertices_[t.iV3].normal += n;
	}
	for (unsigned i=0; i<nVertices_; i++) {
		if ((i/cols_) == 0 || (i/cols_) == rows_-1 || (i%cols_) == 0 || (i%cols_) == cols_-1)
			pVertices_[i].normal = glm::vec3{0.f, 1.f, 0.f};  // we leave the edge vertices at zero to avoid artifacts with the skirt
		else
			pVertices_[i].normal = glm::normalize(pVertices_[i].normal);
	}
}

void Terrain::computeTextureWeights() {
	PerlinNoise pnoise(settings_.width/2, settings_.length/2);
	glm::vec3 topleft {-settings_.width * 0.5f, 0.f, -settings_.length * 0.5f};
	const float grassBias = 0.2f; // bias to more grass over dirt
	for (unsigned i=0; i<rows_*cols_; i++) {
		// grass/rock factor is determined by slope
		// each one of grass and rock have two components blended together by a perlin factor for low-freq variance
		float u = (pVertices_[i].pos.x - topleft.x) / settings_.width * 0.15;
		float v = (pVertices_[i].pos.z - topleft.z) / settings_.length * 0.15;
		pVertices_[i].texBlendFactor.x = grassBias 
											+ pnoise.getNorm(u, v, 7.f) 
											+ 0.3f * pnoise.get(u*2, v*2, 7.f) 
											+ 0.1 * pnoise.get(u*4, v*4, 2.f);	// dirt / grass
		pVertices_[i].texBlendFactor.y = pnoise.getNorm(v, u, 7.f) + 0.5 * pnoise.get(v*4, u*4, 2.f);	// rock1 / rock2
		float cutoffY = 0.80f;	// y-component of normal above which grass is used instead of rock
		// height factor for grass vs rock: the higher the vertex, the more likely it is to be rock
		float hFactor = (pVertices_[i].pos.y - settings_.minElevation) / (settings_.maxElevation - settings_.minElevation);
		hFactor = pow(hFactor, 1.5f);	// hFactor is 1.0 at the highest elevation, 0.0 at the lowest.
		cutoffY += (1.0 - cutoffY) * hFactor;
		pVertices_[i].texBlendFactor.z = pVertices_[i].normal.y > cutoffY ? 1.f : 0.f; // grass vs rock
		
		// sand factor -> some distance above water level and everything below is sand
		float beachHeight = 1.f + 1.5f * pnoise.getNorm(u*10, v*10, 1.f); // meters
		if (pVertices_[i].pos.y < settings_.seaLevel + beachHeight) {
			float sandFactor = min(1.f, settings_.seaLevel + beachHeight - pVertices_[i].pos.y);
			pVertices_[i].texBlendFactor.w = pow(sandFactor, 1.5f);
		}
	}
}

void Terrain::updateRenderBuffers() {
	glBindBuffer(GL_ARRAY_BUFFER, renderData_->VBO_);
	glBufferData(GL_ARRAY_BUFFER, nVertices_ * sizeof(TerrainVertex), pVertices_, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	uint32_t *indices = (uint32_t*)malloc(3 * triangles_.size() * sizeof(uint32_t));
	for (unsigned i=0; i<triangles_.size(); i++) {
		indices[i*3 + 0] = triangles_[i].iV1;
		indices[i*3 + 1] = triangles_[i].iV2;
		indices[i*3 + 2] = triangles_[i].iV3;
	}
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderData_->IBO_);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 3 * triangles_.size() * sizeof(uint32_t), indices, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	free(indices);
}

void Terrain::updatePhysics() {
	// create ground body
	/*if (!physicsBody_) {
		rp3d::Vector3 gPos(0.f, 0.f, 0.f);
		rp3d::Quaternion gOrient = rp3d::Quaternion::identity();
		physicsBody_ = World::getGlobal<rp3d::DynamicsWorld>()->createRigidBody({gPos, gOrient});
		physicsBody_->setType(rp3d::BodyType::STATIC);
		physicsBody_->getMaterial().setBounciness(0.2f);
		physicsBody_->getMaterial().setFrictionCoefficient(0.5);
	}
	if (physicsShapeProxy_) {
		physicsBody_->removeCollisionShape(physicsShapeProxy_);
		delete physicsShape_;
		free(heightFieldValues_);
	}*/
	// create array of height values:
	heightFieldValues_ = (float*)malloc(sizeof(float) * rows_ * cols_);
	for (unsigned i=0; i<rows_; i++)
		for (unsigned j=0; j<cols_; j++)
			heightFieldValues_[i*cols_+j] = pVertices_[i*cols_+j].pos.y;
	// create ground shape
	/*rp3d::Vector3 vScale {gridSpacing_.first, 1.f, gridSpacing_.second};
	physicsShape_ = new rp3d::HeightFieldShape(cols_, rows_, settings_.minElevation, settings_.maxElevation,
								heightFieldValues_, rp3d::HeightFieldShape::HeightDataType::HEIGHT_FLOAT_TYPE,
								1, 1.f, vScale);
	rp3d::Vector3 shapeOffs(0.f, (settings_.maxElevation + settings_.minElevation)*0.5f, 0.f);
	rp3d::Quaternion shapeOrient = rp3d::Quaternion::identity();
	rp3d::Transform shapeTr {shapeOffs, shapeOrient};
	physicsShapeProxy_ = physicsBody_->addCollisionShape(physicsShape_, shapeTr, 1.f);*/
}

void Terrain::draw(Viewport* vp) {
	if (renderWireframe_) {
		glLineWidth(2.f);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}
	// set-up textures
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, renderData_->textures_[0].texID);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, renderData_->textures_[1].texID);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, renderData_->textures_[2].texID);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, renderData_->textures_[3].texID);
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, renderData_->textures_[4].texID);
	// configure backface culling
	glFrontFace(GL_CW);
	glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);
	// set-up shader, vertex buffer and uniforms
	glUseProgram(renderData_->shaderProgram_);
	glUniformMatrix4fv(renderData_->imPV_, 1, GL_FALSE, glm::value_ptr(vp->camera()->matProjView()));
	for (unsigned i=0; i<TerrainVertex::nTextures; i++)
		glUniform1i(renderData_->iSampler_ + i, i);
	glBindVertexArray(renderData_->VAO_);
	// do the drawing
	glDrawElements(GL_TRIANGLES, triangles_.size() * 3, GL_UNSIGNED_INT, nullptr);
	// unbind stuff
	glBindVertexArray(0);
	glUseProgram(0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);

	// draw vertex normals
	/*for (unsigned i=0; i<nVertices_; i++) {
		Shape3D::get()->drawLine(pVertices_[i].pos, pVertices_[i].pos+pVertices_[i].normal, {1.f, 0, 1.f});
	}*/
	
	if (!renderWireframe_)
		pWater_->draw(vp);
	
	if (renderWireframe_) {	// reset state
		glLineWidth(1.f);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
}
