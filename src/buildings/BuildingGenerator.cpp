#include "BuildingGenerator.h"
#include "../terrain/Terrain.h"
#include "../imgUtil/blur.h"
#include "../ImgDebugDraw.h"

#include <boglfw/utils/rand.h>
#include <boglfw/entities/Box.h>
#include <boglfw/World.h>

// blurs a height field over a given radius
/*void blurHeightField(const float* in, int rows, int cols, float radius, float* out) {
	int irad = round(radius);
	float rsq = sqr(radius);
	for (int i=0; i<rows; i++) {
		for (int j=0; j<cols; j++) {
			float val = 0;
			float denom = 0;
			for (int ii=-irad; ii<=irad; ii++) {
				for (int jj=-irad; jj<=irad; jj++) {
					float cubicFactor = (rsq - sqr(ii)+sqr(jj)) / rsq;
					if (cubicFactor <= 0)
						continue;	// the point is outside blur radius
					int ri = i + ii;	// row index
					int ci = j + jj;	// column index
					if (ri < 0 || ri >= rows || ci < 0 || ci >= cols)
						continue; // we're outside the field
					val += in[ri*cols + ci] * cubicFactor;
					denom += cubicFactor;
				}
			}
			out[i*cols + j] = val / denom;
		}
	}
}*/

void downsampleHeightField(const float* in, glm::ivec2 const& inSize, glm::ivec2 const& outSize, float* out) {
	glm::vec2 rcSize = { inSize.x / (float)outSize.x, inSize.y / (float)outSize.y }; // rectangle size for sampling from input
	glm::ivec2 rcCoverage { ceil(rcSize.x), ceil(rcSize.y) };
	glm::vec2 sampleOrigin {0.f, 0.f};
	for (int i=0; i<outSize.y; i++) {
		sampleOrigin.x = 0.f;
		for (int j=0; j<outSize.x; j++) {
			float value = 0.f;
			float denom = 0.f;
			for (float sx=sampleOrigin.x; sx <= sampleOrigin.x+rcSize.x; sx++)
				for (float sy=sampleOrigin.y; sy <= sampleOrigin.y+rcSize.y; sy++) {
					int isx = floor(sx);
					int isy = floor(sy);
					float weight = 1.f;
					if (isx < sampleOrigin.x) {
						// partial coverage left column
						weight *= isx + 1 - sx;
					} else if (isx == j + rcCoverage.x) {
						// partial coverage right column
						weight *= sx - isx;
					}
					if (isy < sampleOrigin.y) {
						// partial coverage top row
						weight *= isy + 1 - sy;
					} else if (isy == i + rcCoverage.y) {
						// partial coverage bottom row
						weight *= sy - isy;
					}
					value += in[isy * inSize.x + isx] * weight;
					denom += weight;
				}
			out[i*outSize.x + j] = value / denom;
			sampleOrigin.x += rcSize.x;
		}
		sampleOrigin.y += rcSize.y;
	}
}

void BuildingGenerator::generate(BuildingsSettings const& settings, Terrain &terrain) {
	glm::ivec2 gridSize = terrain.getGridSize();
	glm::vec2 gridPointDensity { gridSize.x / terrain.getConfig().width, gridSize.y / terrain.getConfig().length };	// grid points per meter
	float desiredGridPointDensity = 1.f / 4;
	glm::ivec2 sourceGridSize = gridSize;
	if (desiredGridPointDensity < gridPointDensity.x || desiredGridPointDensity < gridPointDensity.y) {
		// downsample the heightfield
		float xfactor = min(1.f, desiredGridPointDensity / gridPointDensity.x);
		float yfactor = min(1.f, desiredGridPointDensity / gridPointDensity.y);
		gridSize = glm::ivec2{gridSize.x * xfactor, gridSize.y * yfactor};
	}
	float* fHeights = (float*)malloc(sizeof(float) * gridSize.x * gridSize.y);
	downsampleHeightField(terrain.getHeightField(), sourceGridSize, gridSize, fHeights);
	float samplePointDensity = 0.0045f; // per meter squared -> this will yield a distance of about 15 meters between sample points
	// prepare heightfield for finding locations:
	float blurRadius = 1.f / sqrt(samplePointDensity) * 0.25f;	// [m] one quarter of the distance between sample points
	blurRadius *= gridSize.x / terrain.getConfig().width;
	float* fHeightsBlured = (float*)malloc(sizeof(float) * gridSize.x * gridSize.y);
	imgUtil::blur(fHeights, gridSize.y, gridSize.x, blurRadius, fHeightsBlured);
	free(fHeights), fHeights = nullptr;

	// find suitable locations for castles
	unsigned nSamplePoints = terrain.getConfig().width * terrain.getConfig().length * samplePointDensity;
	glm::ivec2* samplePoints = (glm::ivec2*)malloc(sizeof(glm::ivec2) * nSamplePoints);
	for (unsigned i=0; i<nSamplePoints; i++) {
		glm::ivec2 sp { randi(gridSize.x - 1), randi(gridSize.y - 1) };
		bool seek = true; // seek the local maximum
		while (seek) {
			// look at the four neighbours and move towards the higher one
			glm::ivec2 neighbours[] {
				{max(0, sp.x - 1), sp.y},
				{min(gridSize.x-1, sp.x + 1), sp.y},
				{sp.x, max(0, sp.y - 1)},
				{sp.x, min(gridSize.y-1, sp.y + 1)},
			};
			float nh[] {
				fHeightsBlured[neighbours[0].y*gridSize.x + neighbours[0].x],
				fHeightsBlured[neighbours[1].y*gridSize.x + neighbours[1].x],
				fHeightsBlured[neighbours[2].y*gridSize.x + neighbours[2].x],
				fHeightsBlured[neighbours[3].y*gridSize.x + neighbours[3].x],
			};
			int nmax = 0;
			for (int i=1; i<4; i++)
				if (nh[i] > nh[nmax])
					nmax = i;
			if (nh[nmax] > fHeightsBlured[sp.y * gridSize.x + sp.x]) {
				sp = neighbours[nmax];
			} else
				seek = false;
		}
		samplePoints[i] = sp;

		// debug sample points with boxes:
		glm::vec3 wp { (sp.x / (float)gridSize.x - 0.5f) * terrain.getConfig().width, 0.f,
						(sp.y / (float)gridSize.y - 0.5f) * terrain.getConfig().length };
		wp.y = fHeightsBlured[sp.y * gridSize.x + sp.x];

		std::shared_ptr<Box> spB = std::make_shared<Box>(0.2f, 1.f, 0.2f);
		spB->getTransform().setPosition(wp);
		World::getInstance().takeOwnershipOf(spB);
	}
	// now discard duplicate sample points and use remaining ones to decide where to place castles
	// ...

	free(samplePoints), samplePoints = nullptr;
	free(fHeightsBlured), fHeightsBlured = nullptr;
}
