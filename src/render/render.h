#ifndef RENDER_H
#define RENDER_H

#include "CustomRenderContext.h"

#include <boglfw/renderOpenGL/Viewport.h>
#include <boglfw/renderOpenGL/Camera.h>
#include <boglfw/renderOpenGL/shader.h>
#include <boglfw/renderOpenGL/DefaultShaderPreprocessor.h>
#include <boglfw/renderOpenGL/RenderHelpers.h>
#include <boglfw/renderOpenGL/glToolkit.h>
#include <boglfw/renderOpenGL/Framebuffer.h>
#include <boglfw/utils/assert.h>

#include <glm/fwd.hpp>

class SkyBox;
class Terrain;

struct PostProcessData {
	unsigned VAO = 0;
	unsigned VBO = 0;
	unsigned IBO = 0;
	unsigned shaderProgram = 0;
	int iTexSampler = 0;
	int iUnderwater = 0;
	int iTexSize = 0;
	int iTime = 0;

	glm::vec2 textureSize;
};

struct WaterRenderData {
	FrameBufferDescriptor refractionFBDesc;
	FrameBufferDescriptor reflectionFBDesc;
	FrameBuffer refractionFramebuffer;
	FrameBuffer reflectionFramebuffer;

	glm::vec3 waterColor {0.06f, 0.16f, 0.2f};
};

struct RenderConfig {
	bool renderWireFrame = false;
	bool renderPhysicsDebug = false;
};

struct RenderData {
	RenderConfig config;
	Viewport viewport;
	CustomRenderContext renderCtx;
	unsigned windowW = 0;
	unsigned windowH = 0;
	int defaultFrameBuffer = 0;

	// assign your own function to this to perform debug drawing after postprocessing
	std::function<void(RenderContext const& ctx)> drawDebugData = nullptr;

	WaterRenderData waterRenderData;
	PostProcessData postProcessData;

	DefaultShaderPreprocessor shaderPreprocessor;

	SkyBox *pSkyBox = nullptr;
	Terrain *pTerrain = nullptr;

	RenderData(unsigned winW, unsigned winH)
		: viewport(0, 0, winW, winH)
		, renderCtx()
		, windowW(winW)
		, windowH(winH)
		{
			renderCtx.pViewport = &viewport;
		}

	~RenderData() {
		assertDbg(dependenciesUnloaded_ && "call unloadDependencies() before destroying this");
	}

private:
	friend bool initRender(const char*, RenderData&);
	friend void unloadRender(RenderData &renderData);

	void setupDependencies();
	void unloadDependencies();

	bool dependenciesUnloaded_ = true;
};

bool initRender(const char* winTitle, RenderData &renderData);
void unloadRender(RenderData &renderData);
void render(RenderData &renderData);


#endif // RENDER_H
