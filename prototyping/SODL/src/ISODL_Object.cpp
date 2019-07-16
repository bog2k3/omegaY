#include "ISODL_Object.h"
#include "strcat.h"

// constructs a descriptor for a simple value type property
SODL_Property_Descriptor::SODL_Property_Descriptor(SODL_Value::Type valueType, void* valuePtr)
	: isObject(false), type(valueType), valueOrCallbackPtr(valuePtr) {
}

// constructs a descriptor for an object type property
SODL_Property_Descriptor::SODL_Property_Descriptor(std::string objectType, std::shared_ptr<ISODL_Object> *objectPtr)
	: isObject(true), valueOrCallbackPtr(objectPtr) {
	objectTypes.push_back({objectType, objectType});
}

// constructs a descriptor for an object type property that can accept one of multiple object types
SODL_Property_Descriptor::SODL_Property_Descriptor(std::vector<std::pair<std::string, std::string>> objectTypes, std::shared_ptr<ISODL_Object> *objectPtr)
	: isObject(true), objectTypes(objectTypes), valueOrCallbackPtr(objectPtr) {
}

// constructs a descriptor for a callback (std::function<void(argTypes...)>)
SODL_Property_Descriptor::SODL_Property_Descriptor(void* funcPtr, std::vector<SODL_Value::Type> argTypes)
	: isObject(false), type(SODL_Value::Type::Callback)
	, callbackArgTypes(argTypes), valueOrCallbackPtr(funcPtr) {
}

// defines a "primary" (mandatory) property; primary property values can be written directly in the element's declaration header
void ISODL_Object::definePrimaryProperty(const char* name, SODL_Value::Type type, void* valuePtr) {
	primaryPropertyDesc_.push_back({type, valuePtr});
	mapPropertyDesc_[name] = {type, valuePtr};
}

// defines a "secondary" (optional) property; these must be defined as name: value within the element's block
void ISODL_Object::defineSecondaryProperty(const char* name, SODL_Property_Descriptor desc) {
	mapPropertyDesc_[name] = desc;
}

SODL_result ISODL_Object::setPrimaryProperty(unsigned index, SODL_Value const& val) {
	return SODL_result::error("not implemented");
}

SODL_result ISODL_Object::instantiateClass(std::string const& className, std::shared_ptr<ISODL_Object> &out_pInstance) {
	return SODL_result::error("not implemented");
}

SODL_result ISODL_Object::addChildObject(std::shared_ptr<ISODL_Object> pObj) {
	return SODL_result::error("not implemented");
}

SODL_result ISODL_Object::setProperty(std::string const& propName, SODL_Value const& val) {
	return SODL_result::error("not implemented");
}

SODL_result ISODL_Object::setProperty(std::string const& propName, std::shared_ptr<ISODL_Object> objPtr) {
	return SODL_result::error("not implemented");
}

SODL_result ISODL_Object::describePrimaryProperty(unsigned index, SODL_Property_Descriptor &out_desc) {
	if (index >= primaryPropertyDesc_.size())
		return SODL_result::error(strcat() << "Invalid primary property index: " << index);
	out_desc = primaryPropertyDesc_[index];
	return SODL_result::OK();
}

SODL_result ISODL_Object::describeProperty(std::string const& propName, SODL_Property_Descriptor &out_desc) {
	auto it = mapPropertyDesc_.find(propName);
	if (it == mapPropertyDesc_.end())
		return SODL_result::error(strcat() << "Unknown property \"" << propName << "\"");
	out_desc = it->second;
	return SODL_result::OK();
}
	
