#pragma once

#include "../../ISODL_Object.h"

class SliderSODLWrapper : public ISODL_Object {
public:
	const std::string objectType() const override { return "slider"; }
	~SliderSODLWrapper() override {}
};
