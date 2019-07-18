#include "LayoutSODLWrapper.h"

#include <boglfw/GUI/FillLayout.h>
#include <boglfw/GUI/FreeLayout.h>
#include <boglfw/GUI/GridLayout.h>
#include <boglfw/GUI/ListLayout.h>
#include <boglfw/GUI/SplitLayout.h>

#include <cstring>

class LayoutSODLWrapper::Impl {
public:
	enum layoutType : int32_t {
		fill,
		free,
		grid,
		list,
		split
	} type;

	enum eDirection : int32_t {
		horizontal,
		vertical
	};

	enum eHAlign : int32_t {
		left,
		right,
		center
	};
	enum eVAlign : int32_t {
		top,
		bottom,
		middle
	};

	bool isSet = false;
	std::shared_ptr<Layout> pLayout;

	struct {
		std::shared_ptr<FillLayout> ptr;
	} fillProps;

	struct {
		std::shared_ptr<FreeLayout> ptr;
	} freeProps;

	struct {
		std::shared_ptr<GridLayout> ptr;
	} gridProps;

	struct {
		std::shared_ptr<ListLayout> ptr;
		eDirection direction = vertical;
		eHAlign align = left;
		eVAlign vertAlign = top;
	} listProps;

	struct {
		std::shared_ptr<SplitLayout> ptr;
		std::shared_ptr<LayoutSODLWrapper> splitFirst;
		std::shared_ptr<LayoutSODLWrapper> splitSecond;
		eDirection direction = horizontal;
		FlexCoord offset;
		float splitPoint;
	} splitProps;

	void create() {
		switch (type) {
		case fill:
			pLayout = fillProps.ptr = std::make_shared<FillLayout>();
			break;
		case free:
			pLayout = freeProps.ptr = std::make_shared<FreeLayout>();
			break;
		case grid:
			pLayout = gridProps.ptr = std::make_shared<GridLayout>();
			break;
		case list:
			pLayout = listProps.ptr = std::make_shared<ListLayout>();
			break;
		case split:
			pLayout = splitProps.ptr = std::make_shared<SplitLayout>();
			break;
		default:
			assertDbg(false && "invalid layout type");
		}
		isSet = true;
	};

	void applyProps() {

	}
};

LayoutSODLWrapper::LayoutSODLWrapper()
	: pImpl_(new Impl()) {
	defineEnum("enumLayoutType", {
		"fill",
		"free",
		"grid",
		"list",
		"split"
	});
	definePrimaryProperty("type", {SODL_Value::Type::Enum, "enumLayoutType"});
	// the rest of the properties are defined dynamically depending on this first one
	// see setUserPropertyValue

	loadingFinished.add(std::bind(&LayoutSODLWrapper::onLoadingFinished, this));
}

bool LayoutSODLWrapper::setUserPropertyValue(const char* propName, int32_t enumVal) {
	if (!strcmp(propName, "type")) {
		if (pImpl_->isSet)
			return false; // type already set, we only allow it once
		pImpl_->type = (Impl::layoutType)enumVal;
		pImpl_->create();
		defineEnums();
		switch (pImpl_->type) {
			case Impl::fill:
				defineFillProps();
				break;
			case Impl::free:
				defineFreeProps();
				break;
			case Impl::grid:
				defineGridProps();
				break;
			case Impl::list:
				defineListProps();
				break;
			case Impl::split:
				defineSplitProps();
				break;
		}
		return true;
	} else
		return false;
}

void LayoutSODLWrapper::defineEnums() {
	defineEnum("enumDirection", {
		"horizontal",
		"vertical"
	});
	defineEnum("enumAlign", {
		"left",
		"right",
		"center"
	});
	defineEnum("enumVertAlign", {
		"top",
		"bottom",
		"middle"
	});
}

void LayoutSODLWrapper::defineFillProps() {

}

void LayoutSODLWrapper::defineFreeProps() {

}

void LayoutSODLWrapper::defineGridProps() {

}

void LayoutSODLWrapper::defineListProps() {
	definePrimaryProperty("direction", {"enumDirection", (int32_t&)pImpl_->listProps.direction});
	defineSecondaryProperty("align", {"enumAlign", (int32_t&)pImpl_->listProps.align});
	defineSecondaryProperty("vertAlign", {"enumVertAlign", (int32_t&)pImpl_->listProps.vertAlign});
}

void LayoutSODLWrapper::defineSplitProps() {
	definePrimaryProperty("direction", {"enumDirection", (int32_t&)pImpl_->splitProps.direction});
	definePrimaryProperty("offset", {pImpl_->splitProps.offset});
	definePrimaryProperty("splitPoint", {pImpl_->splitProps.splitPoint});

	defineSecondaryProperty("first", {"layout", (std::shared_ptr<ISODL_Object>*)&pImpl_->splitProps.splitFirst});
	defineSecondaryProperty("second", {"layout", (std::shared_ptr<ISODL_Object>*)&pImpl_->splitProps.splitSecond});
}

std::shared_ptr<Layout> LayoutSODLWrapper::get() const {
	return pImpl_->pLayout;
}

void LayoutSODLWrapper::onLoadingFinished() {
	pImpl_->applyProps();
}
