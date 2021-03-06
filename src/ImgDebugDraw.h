#ifndef __IMG_DEBUG_DRAW_H__
#define __IMG_DEBUG_DRAW_H__

class RenderContext;

class ImgDebugDraw {
public:
	ImgDebugDraw();
	~ImgDebugDraw();

	enum PixelFormat {
		FMT_RGB,		// 3 component rgb floating point
		FMT_XY,			// 2 component XY floating point
		FMT_GRAYSCALE,	// 1 component grayscale floating point
	};

	enum FilterMode {
		FILTER_NEAREST,
		FILTER_LINEAR,
	};

	void setValues(const float* values, int width, int height, float rangeMin, float rangeMax, PixelFormat fmt, FilterMode filter=FILTER_LINEAR);
	void enable() { enabled_ = true; }
	void disable() { enabled_ = false; }

	void draw(RenderContext const& ctx);

private:
	struct RenderData;

	bool enabled_ = false;
	PixelFormat pixelFormat_;
	RenderData* pRenderData_ = nullptr;
};

#endif // __IMG_DEBUG_DRAW_H__
