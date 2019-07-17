#include "ContainerSODLWrapper.h"
#include "../common/Coord2SODLWrapper.h"
#include "../common/Coord4SODLWrapper.h"

ContainerSODLWrapper::ContainerSODLWrapper()
	: container_(new GuiContainerElement()) {
	setupCommonProperties(container_);

	defineSecondaryProperty("size", {"coord2", (std::shared_ptr<ISODL_Object>*)&size_});
	defineSecondaryProperty("padding", {"coord4", (std::shared_ptr<ISODL_Object>*)&padding_});
	defineSecondaryProperty("layout", {"layout", (std::shared_ptr<ISODL_Object>*)&layout_});

	defineSupportedChildTypes({	GuiElementSODLWrapper::objectType() });

	loadingFinished.add(std::bind(&ContainerSODLWrapper::onLoadingFinished, this));
}

void ContainerSODLWrapper::onLoadingFinished() {
	if (size_ != nullptr)
		container_->setSize(size_->get());
	if (padding_ != nullptr) {
		container_->setClientArea(padding_->getTopLeft(), padding_->getBottomRight());
	}
	container_->useLayout(layout_->get());
}

bool ContainerSODLWrapper::addChildObject(std::shared_ptr<ISODL_Object> pObj) {
	auto pElement = std::dynamic_pointer_cast<GuiElementSODLWrapper>(pObj);
	if (!pElement || !pElement->get())
		return false;
	container_->addElement(pElement->get());
	return true;
}
