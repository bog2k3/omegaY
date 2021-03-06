#pragma once

/*
	SODL_loader.h

	@author: bogdan ionita <bog2k3@gmail.com>
	@date: July 7, 2019

	SODL - Simplified Object Description Language
	This class implements an object loader from a SODL file
*/

#include "ISODL_Object.h"
#include "SODL_common.h"

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

class SODL_Loader {
public:
	SODL_Loader(ISODL_Object_Factory &factory)
		: factory_(factory) {
	}
	~SODL_Loader();

	// registers a new data binding that can be referenced from the SODL that will be loaded
	// supported types are:
	//		String (std::string)
	//		Number (float)
	//		Coordinate (FlexCoord)
	//		Bool (bool)
	template<class DataType>
	void addDataBinding(const char* name, DataType &data);

	// registers an action callback that can be referenced from the SODL that will be loaded
	template <class FuncType>
	void addActionBinding(const char* name, std::vector<SODL_Value::Type> argumentTypes, std::function<FuncType> func);

	// registers an event to bind to the named action; when some element invokes the named action the event will be triggered.
	template<class... EventArgs>
	void addActionBinding(const char* name, std::vector<SODL_Value::Type> argTypes, Event<void(EventArgs...)> &event);

	// loads a SODL file and returns a new ISODL_Object (actual type depending on the root node's type in the file)
	SODL_result loadObject(const char* filename, std::shared_ptr<ISODL_Object> &out_pObj);

	// merges a SODL file with an existing ISODL_Object provided by user; The root node in the file mustn't specify a type.
	SODL_result mergeObject(ISODL_Object &object, const char* filename);

private:
	class ParseStream;
	class CharToLineMapping;
	struct _SODL_Loader_ActionBindingDescriptor;

	ISODL_Object_Factory &factory_;
	std::unordered_map<std::string, std::pair<SODL_Value::Type, void*>> mapDataBindings_;
	std::unordered_map<std::string, _SODL_Loader_ActionBindingDescriptor*> mapActionBindings_;
	std::unordered_map<std::string, std::shared_ptr<ISODL_Object>> mapClasses_;

	std::pair<char*, size_t> readFile(const char* fileName, CharToLineMapping &outMapping);

	// remove all comments and reduce all whitespace to a single ' ' char;
	// the output buffer must have at least the same size as the input buffer;
	// returns the size of the preprocessed data
	size_t preprocess(const char* input, size_t length, char* output, CharToLineMapping &outMapping);

	SODL_result loadObjectImpl(ParseStream &stream, std::string *pObjType, std::shared_ptr<ISODL_Object> &out_pObj);
	SODL_result mergeObjectImpl(ISODL_Object &object, ParseStream &stream);

	SODL_result readObjectType(ParseStream &stream, std::string &out_type);
	SODL_result instantiateObject(std::string const& objType, std::shared_ptr<ISODL_Object> &out_pObj);
	SODL_result readPrimaryProps(ISODL_Object &object, ParseStream &stream);
	SODL_result readObjectBlock(ISODL_Object &object, ParseStream &stream);
	SODL_result readClass(ISODL_Object &object, ParseStream &stream);

	SODL_result resolveDataBinding(SODL_Value &inOutVal, SODL_Value::Type expectedType, unsigned crtLine);
	SODL_result checkCallbackArgumentsMatch(std::vector<SODL_Value::Type> argTypes, std::vector<SODL_Value::Type> expectedTypes, unsigned crtLine);
	SODL_result assignPropertyValue(ISODL_Object &object, SODL_Property_Descriptor const& desc,
									SODL_Value& val, unsigned primaryPropIdx, std::string propName, unsigned crtLine);
	bool objectSupportsChildType(ISODL_Object &object, std::string const& typeName);
	bool canConvertAssignType(SODL_Value::Type from, SODL_Value::Type to);
	SODL_result addClassDefinition(std::string const& className, std::shared_ptr<ISODL_Object> pClassObj, unsigned crtLine);
	SODL_result instantiateClass(std::string const& className, std::shared_ptr<ISODL_Object> &out_pInstance, unsigned crtLine);

	void addDataBindingImpl(const char* name, SODL_Value::Type type, void* valuePtr);
};

#include "SODL_loader_private.h"
