#include "SODL_loader.h"
#include "ISODL_Object.h"
#include <boglfw/utils/strbld.h>

#include <boglfw/utils/filesystem.h>
#include <boglfw/utils/assert.h>

#include <fstream>
#include <cmath>

/*
	SODL file structure
	--------------------------------------------

	the file must contain a single root node (if it contains more, the next ones after the 1st are ignored)
	each node can contain child nodes.

	comments are C-style line comments with // ...
	there are no special characters to terminate property or node definition, end-of-line is used instead
	(or {} in the case of blocks)
	whitespace doesn't have any meaning appart from separating values
	colon (:) is used to separate property name from value within blocks

	definition of a node:

	[node type] [opt: node id] [opt: node primary properties values] [opt: {]
		[opt: secondary or primary properties that were not already defined in the header]
		[opt: child nodes follow the same definition rules]
	[opt: } if the block was opened, this is mandatory]

	node type is specified as a single camelCase word (ex: container)
		For the root node, the type may be omitted, and in this case the file can not be loaded with
			loadObject because this one needs to know the node type to instantiate the correct object
		instead it can be loaded with
			mergeObject passing in a user-instantiated object that should be of the correct type

	node id must begin with a # (all ids begin with #) (ex: #rootNode)
	primary properties defined in the header are value only (ex: "someString" 13 42% enumValue)
	secondary properties are defined as camelCase identifier followed by colon (:) and value (ex: size: 12 50%)
	some secondary properties may be nodes themselves with id, primary/secondary properties and body (ex:
		layout: list #listId {
			align: right
		}
	)

	class definitions:

	class className nodeType [primaryPropValues] {
		secondaryProps: ...
		childNodes { ... }
	}

	A class is just a regular node that can be instantiated multiple times and augmented with more properties and children each time
	To instantiate a class the @ operator is used:

	@className {
		secondaryProp: 13
		childType #child1 "primaryPropVal" {
			...
		}
	}
	note that there can be no primary properties in header of a class instantiation, they must be defined in the body with "name: value" syntax

	example file : ------------------------------------

	#root {
		size: 100% 100%		// values can be absolute or relative
		padding: 50 50 50 50 // top, right, bottom, left
		layout: split vertical 10% 2 {
			first: fill
			second: fill
		}
		label #title "Host Game" 50 center	// one line child with no block
		picture #TerrainPicture
		container #controls {			// container is the node type
			layout: list {
				align: right
			}
			class controlRow container {	// class definition - a class of type container with name controlRow
				height: 50					// class has properties and even child nodes that are inherited by all instantiations
				layout: list horizontal {
					align: right
					vertAlign: bottom
				}
			}
			@controlRow {						// class instantiation
				label "Seed" 18
				textField numeric $terrainSeed {	// everything that starts with $ is a data/callback binding
					width: 150
					onChange: $seedChanged
				}
				button "Random" $randSeed
			}
		}
	}
	// end of file

 */

class SODL_Loader::CharToLineMapping {
public:
	unsigned getLine(unsigned charLocation) {
		auto it = anchors_.begin();
		unsigned lineNr = 0;
		while (it < anchors_.end()) {
			if (it->first <= charLocation) {
				lineNr = it->second;
				++it;
			} else
				break;
		}
		return lineNr;
	}

	void addAnchor(unsigned charLocation, unsigned lineNr) {
		anchors_.emplace_back(charLocation, lineNr);
	}

private:
	std::vector<std::pair<unsigned, unsigned>> anchors_;
};

static bool isWhitespace(const char c) {
	return c == ' ' || c == '\t' || c == '\r';
}

static bool isEOL(const char c) {
	return c == '\n';
}

class SODL_Loader::ParseStream {
public:
	ParseStream(const char* buf, size_t length, SODL_Loader::CharToLineMapping &lineMapping)
		: bufStart_(buf)
		, bufCrt_(buf)
		, bufEnd_(buf + length)
		, lineMapping_(lineMapping)
	{}

	~ParseStream() {}

	unsigned crtLine() const {
		return lineMapping_.getLine(bufCrt_ - bufStart_);
	}

	// returns the next usable char in the stream, looking over whitespace and line ends;
	// does not change the current position of the stream.
	char nextChar() {
		const char* ptr = bufCrt_;
		while (ptr < bufEnd_ && (isWhitespace(*ptr) || isEOL(*ptr)))
			ptr++;
		if (ptr == bufEnd_)
			return 0;
		else
			return *ptr;
	}

	void skipChar(char c) {
		skipWhitespace(true);
		assertDbg(bufCrt_ < bufEnd_ && *bufCrt_ == c);
		bufCrt_++;
		skipWhitespace(false);
	}

	void skipEOL() {
		assertDbg(eol());
		skipWhitespace(true);
	}

	bool eol() {
		return bufCrt_ < bufEnd_ && isEOL(*bufCrt_);
	}

	bool eof() {
		return bufCrt_ >= bufEnd_;
	}

	bool nextIsWhitespace() {
		return !eof() && isWhitespace(*bufCrt_);
	}

	SODL_result readValue(SODL_Value::Type type, SODL_Value &out_val) {
		if (nextChar() == '$') {
			if (type == SODL_Value::Type::Enum)
				return SODL_result::error("Enum properties cannot use bindings, must be immediate values", crtLine());
			skipChar('$');
			out_val.isBinding = true;
			return readIdentifier(out_val.bindingName);
		} else switch (type) {
		case SODL_Value::Type::Callback:
			return SODL_result::error("Expected callback binding to start with $", crtLine());
		case SODL_Value::Type::Coordinate: {
			auto res = readNumber(out_val.numberVal, true);
			if (!res)
				return res.wrap("Reading coordinate value");
			if (!eof() && *bufCrt_ == '%') {
				skipChar('%');
				out_val.isPercentCoord = true;
				skipWhitespace(false);
			}
			return SODL_result::OK();
		}
		case SODL_Value::Type::Enum:
			return readIdentifier(out_val.enumVal);
		case SODL_Value::Type::Number:
			return readNumber(out_val.numberVal, false);
		case SODL_Value::Type::String:
			return readQuotedString(out_val.stringVal);
		case SODL_Value::Type::Bool: {
			std::string boolValueName;
			auto res = readIdentifier(boolValueName);
			if (!res)
				return res.wrap("Reading boolean value");
			if (boolValueName == "true")
				out_val.boolVal = true;
			else if (boolValueName == "false")
				out_val.boolVal = false;
			else
				return SODL_result::error("Invalid value for Bool property (must be one of the literals true or false", crtLine());
			return SODL_result::OK();
		}
		default:
			return SODL_result::error(strbld() << "unknown value type: " << (int)type, crtLine());
		}
	}

	SODL_result readQuotedString(std::string &out_str) {
		skipWhitespace(false);
		if (eol() || eof())
			return SODL_result::error("End of line while expecting a string value", crtLine());
		if (*bufCrt_ != '"')
			return SODL_result::error(strbld() << "Unexpected character while expecting a string value: '" << *bufCrt_ << "'", crtLine());
		bufCrt_++;
		const char* strBegin = bufCrt_;
		const char* strEnd = strBegin;
		while (!eol() && !eof() && *bufCrt_ != '"')
			strEnd++, bufCrt_++;
		if (eol() || eof())
			return SODL_result::error("End of line while reading a string value", crtLine());
		skipChar('"');
		out_str = std::string(strBegin, strEnd);
		return SODL_result::OK();
	}

	SODL_result readNumber(float &out_nr, bool allowPercent) {
		skipWhitespace(false);
		out_nr = 0;
		bool firstDigit = true;
		bool hasDot = false;
		bool negative = false;
		int decimalPlace = 0;
		while (!eof() && !eol() && !nextIsWhitespace()) {
			if (*bufCrt_ == '-') {
				if (!firstDigit)
					return SODL_result::error(strbld() << "Invalid char within number: '-'", crtLine());
				negative = true;
				bufCrt_++;
				continue;
			} else if (*bufCrt_ == '.') {
				if (firstDigit)
					return SODL_result::error("Number cannot begin with decimal dot", crtLine());
				if (hasDot)
					return SODL_result::error("Number cannot contain multiple decimal dots", crtLine());
				hasDot = true;
				decimalPlace = 1;
				bufCrt_++;
				continue;
			} else if (!isValidDigit(*bufCrt_)) {
				if (allowPercent && *bufCrt_ == '%')
					break; // this would be the end of our number
				else
					return SODL_result::error(strbld() << "Invalid char within number: '" << *bufCrt_ << "'", crtLine());

			}
			int digitValue = (*bufCrt_ - '0') * (negative ? -1 : +1);
			if (hasDot) {
				out_nr += digitValue * pow(10, -decimalPlace);
				decimalPlace++;
			} else {
				out_nr = out_nr * 10 + digitValue;
			}
			firstDigit = false;
			bufCrt_++;
		}
		if (firstDigit) {
			// no digits were read
			return SODL_result::error("End of line while expecting a number", crtLine());
		}
		skipWhitespace(false);
		return SODL_result::OK();
	}

	SODL_result readIdentifier(std::string &out_str) {
		skipWhitespace(false);
		const char* idStart = bufCrt_;
		// first char must be treated special
		if (eof())
			return SODL_result::error("Reached end of file while expecting an identifier.", crtLine());
		if (!isValidFirstChar(*bufCrt_))
			return SODL_result::error("Identifier starts with invalid character.", crtLine());
		bufCrt_++;
		while (!eof() && isValidIdentChar(*bufCrt_))
			bufCrt_++;
		out_str = std::string(idStart, bufCrt_);
		skipWhitespace(false);
		return SODL_result::OK();
	}

private:
	const char* bufStart_;
	const char* bufCrt_;
	const char* bufEnd_;
	SODL_Loader::CharToLineMapping &lineMapping_;

	void skipWhitespace(bool skipLineEnd) {
		while (bufCrt_ < bufEnd_ && (isWhitespace(*bufCrt_) || (skipLineEnd && isEOL(*bufCrt_))))
			bufCrt_++;
	}

	// returns true if the char is valid as a first character of an identifier
	bool isValidFirstChar(char c) {
		return (c >= 'a' && c <= 'z')
			|| (c >= 'A' && c <= 'Z')
			|| c == '_';
	}

	// returns true if the char is a valid char in the middle of an identifier
	bool isValidIdentChar(char c) {
		return isValidFirstChar(c)
			|| (c >= '0' && c <= '9');
	}

	bool isValidDigit(char c) {
		return (c >= '0') && (c <= '9');
	}
};

template<class TargetType>
class ValueConverter {};

template<>
class ValueConverter<float> {
public:
	static float convertValue(SODL_Value::Type sourceType, void* sourceVal) {
		switch (sourceType) {
		case SODL_Value::Type::Coordinate:
			return static_cast<FlexCoord*>(sourceVal)->get(FlexCoord::X_LEFT, glm::vec2(100.f, 0));
		case SODL_Value::Type::Number:
			return *static_cast<float*>(sourceVal);
		case SODL_Value::Type::String: {
			float ret;
			sscanf(static_cast<std::string*>(sourceVal)->c_str(), "%f", &ret);
			return ret;
		}
		default:
			return 0.f;
		}
	}
};

template<>
class ValueConverter<FlexCoord> {
public:
	static FlexCoord convertValue(SODL_Value::Type sourceType, void* sourceVal) {
		switch (sourceType) {
		case SODL_Value::Type::Coordinate:
			return *static_cast<FlexCoord*>(sourceVal);
		case SODL_Value::Type::Number:
			return {*static_cast<float*>(sourceVal)};
		case SODL_Value::Type::String: {
			float out;
			std::string *pStr = static_cast<std::string*>(sourceVal);
			sscanf(pStr->c_str(), "%f", &out);
			return {out, pStr->back() == '%' ? FlexCoord::PERCENT : FlexCoord::PIXELS};
		}
		default:
			return {0.f};
		}
	}
};

template<>
class ValueConverter<std::string> {
public:
	static std::string convertValue(SODL_Value::Type sourceType, void* sourceVal) {
		switch (sourceType) {
		case SODL_Value::Type::Coordinate: {
			FlexCoord *pCoord = static_cast<FlexCoord*>(sourceVal);
			return strbld() << pCoord->get(FlexCoord::X_LEFT, glm::vec2(100, 0))
				<< (pCoord->unit() == FlexCoord::PERCENT ? "%" : "");
		}
		case SODL_Value::Type::Number:
			return strbld() << *static_cast<float*>(sourceVal);
		case SODL_Value::Type::String:
			return *static_cast<std::string*>(sourceVal);
		default:
			return "";
		}
	}
};

SODL_Loader::~SODL_Loader() {
	for (auto &pair : mapActionBindings_)
		if (pair.second)
			delete pair.second;
	mapActionBindings_.clear();
}

void SODL_Loader::addDataBindingImpl(const char* name, SODL_Value::Type type, void* valuePtr) {
	mapDataBindings_[name] = {type, valuePtr};
}

// loads a SODL file and returns a new ISODL_Object (actual type depending on the root node's type in the file)
SODL_result SODL_Loader::loadObject(const char* filename, std::shared_ptr<ISODL_Object> &out_pObj) {
	CharToLineMapping lineMapping;
	auto text = readFile(filename, lineMapping);
	if (!text.first || !text.second) {
		return SODL_result::error(strbld() << "Unable to open SODL file: '" << filename << "'");
	}
	ParseStream stream(text.first, text.second, lineMapping);
	return loadObjectImpl(stream, nullptr, out_pObj);
}

// merges a SODL file with an existing ISODL_Object provided by user; The root node in the file mustn't specify a type.
SODL_result SODL_Loader::mergeObject(ISODL_Object &object, const char* filename) {
	CharToLineMapping lineMapping;
	auto text = readFile(filename, lineMapping);
	if (!text.first || !text.second) {
		return SODL_result::error(strbld() << "Unable to open SODL file: '" << filename << "'");
	}
	ParseStream stream(text.first, text.second, lineMapping);
	return mergeObjectImpl(object, stream);
}

std::pair<char*, size_t> SODL_Loader::readFile(const char* fileName, CharToLineMapping &outMapping) {
	if (!filesystem::pathExists(fileName))
		return {nullptr, 0};
	size_t length = filesystem::getFileSize(fileName);
	char* buf = new char[length + 1];
	std::ifstream file(fileName);
	file.read(buf, length);
	buf[length] = 0;
	char* preprocessBuf = new char[length + 1];
	length = preprocess(buf, length, preprocessBuf, outMapping);
	delete [] buf;
	return {preprocessBuf, length};
}

// remove all comments and reduce all whitespace to a single ' ' char
size_t SODL_Loader::preprocess(const char* input, size_t length, char* output, CharToLineMapping &outMapping) {
	auto commentStarts = [](const char *c, const char *end) {
		return c+1 < end && *c == '/' && *(c+1)=='/';
	};

	char* outputStart = output;
	const char* end = input + length;
	const char* ptr = input;
	unsigned crtInputLine = 1;
	bool atLineStart = true;
	while (ptr < end) {
		// process current line
		// 1. skip leading whitespace
		while (ptr < end && isWhitespace(*ptr))
			ptr++;
		// we're now either at the end of file or at the first non-white-space character on the line
		const char* lineStart = ptr;
		if (ptr == end)
			break;
		// if the line was empty, skip it
		if (isEOL(*ptr)) {
			crtInputLine++;
			atLineStart = true;
			ptr++;
			continue;
		}
		bool previousWhitespace = false;
		while (ptr < end && !isEOL(*ptr) && !commentStarts(ptr, end)) {
			// we want to reduce all whitespaces to a single character
			while (ptr < end && isWhitespace(*ptr) && previousWhitespace)
				ptr++;
			if (ptr == end || commentStarts(ptr, end))
				break;
			previousWhitespace = isWhitespace(*ptr);
			if (atLineStart) {
				outMapping.addAnchor(output - outputStart, crtInputLine);
				atLineStart = false;
			}
			*output = previousWhitespace ? ' ' : *ptr, output++, ptr++;
		}
		// check and skip comments
		const char* commentBegining = nullptr;
		if (commentStarts(ptr, end)) {
			commentBegining = ptr;
			while (ptr < end && !isEOL(*ptr))
				ptr++;
			if (ptr == end)
				break;
		}
		assertDbg(ptr == end || isEOL(*ptr));
		if (ptr != end && isEOL(*ptr)) {
			if (commentBegining != lineStart) {
				// if there was something on the line before the comment, we also write the EOL char
				*output = *ptr, output++, ptr++, crtInputLine++;
				atLineStart = true;
			} else {
				ptr++;
			}
		}
	}
	*output = 0; // don't forget the zero terminator
	return output + 1 - outputStart; // return the size of the output
}

SODL_result SODL_Loader::loadObjectImpl(SODL_Loader::ParseStream &stream, std::string *pObjType, std::shared_ptr<ISODL_Object> &out_pObj) {
	// 1. read object type
	// 2. instantiate object
	// 3. call mergeObject on intantiated object with the rest of the buffer
	SODL_result res;
	do {
		std::string objType;
		if (pObjType)
			objType = *pObjType;
		else {
			res = readObjectType(stream, objType);
			if (!res) {
				res = res.wrap("Reading object type");
				break;
			}
		}
		res = instantiateObject(objType, out_pObj);
		if (!res) {
			res = res.wrap(strbld() << "Instantiating object of type " << objType);
			break;
		}
		assertDbg(out_pObj != nullptr);
		res = mergeObjectImpl(*out_pObj, stream);
	} while (0);
	return res;
}

SODL_result SODL_Loader::mergeObjectImpl(ISODL_Object &object, SODL_Loader::ParseStream &stream) {
	SODL_result res;
	do {
		res = readPrimaryProps(object, stream);
		if (!res)
			break;
		if (stream.nextChar() == '{')
			res = readObjectBlock(object, stream);
	} while (0);
	if (res)
		object.loadingFinished.trigger();
	else
		res = res.wrap(strbld() << "Reading object of type: " << object.objectType());
	return res;
}

SODL_result SODL_Loader::readObjectType(SODL_Loader::ParseStream &stream, std::string &out_type) {
	return stream.readIdentifier(out_type);
}

SODL_result SODL_Loader::instantiateObject(std::string const& objType, std::shared_ptr<ISODL_Object> &out_pObj) {
	SODL_result res = factory_.constructObject(objType, out_pObj);
	if (!res)
		res = res.wrap(strbld() << "Failed to construct object of type [" << objType << "]");
	return res;
}

SODL_result SODL_Loader::assignPropertyValue(ISODL_Object &object, SODL_Property_Descriptor const& desc,
	SODL_Value& val, unsigned primaryPropIdx, std::string propName, unsigned crtLine) {
	SODL_result res;
	do {
		if (val.isBinding) {
			if (desc.type == SODL_Value::Type::Callback) {
				// retrieve the registered action callback for this binding and set it onto the object
				auto it = mapActionBindings_.find(val.bindingName);
				if (it == mapActionBindings_.end())
					return SODL_result::error(strbld() << "Action $" << val.bindingName << " was not defined.", crtLine);
				auto &actionDesc = *it->second;
				res = checkCallbackArgumentsMatch(actionDesc.argTypes_, desc.callbackArgTypes, crtLine);
				if (!res)
					break;
				actionDesc.pBindingWrapper_->setObjectCallbackBinding(desc.valueOrCallbackPtr, desc.isEvent);
			} else {
				// retrieve the registered data binding and set its value onto the object
				res = resolveDataBinding(val, desc.type, crtLine);
				if (!res)
					break;
				if (propName.empty())
					res = object.setPrimaryProperty(primaryPropIdx, val);
				else
					res = object.setProperty(propName, val);
				if (!res) {
					res.addLineInfo(crtLine);
					break;
				}
			}
		} else if (propName.empty())
			res = object.setPrimaryProperty(primaryPropIdx, val);
		else
			res = object.setProperty(propName, val);
		if (!res)
			res.addLineInfo(crtLine);
	} while (0);
	if (!res)
		res = res.wrap(strbld() << "Assigning value to property '" << propName << "'");
	return res;
}

SODL_result SODL_Loader::readPrimaryProps(ISODL_Object &object, SODL_Loader::ParseStream &stream) {
	SODL_result res = SODL_result::OK();
	do {
		if (stream.nextChar() == '#') {
			stream.skipChar('#');
			if (stream.nextIsWhitespace()) {
				return SODL_result::error("Expected identifier after #, found white space", stream.crtLine());
			}
			std::string idStr;
			res = stream.readIdentifier(idStr);
			if (!res)
				break;
			object.setId(idStr);
		}
		unsigned propIdx = 0;
		while (!stream.eof() && !stream.eol() && stream.nextChar() != '{') {
			SODL_Property_Descriptor desc;
			res = object.describePrimaryProperty(propIdx, desc);
			if (!res) {
				res.addLineInfo(stream.crtLine());
				break;
			}
			assertDbg(!desc.isObject);
			SODL_Value val;
			res = stream.readValue(desc.type, val);
			if (res)
				res = assignPropertyValue(object, desc, val, propIdx, "", stream.crtLine());
			if (!res) {
				res = res.wrap(strbld() << "Primary property position: " << propIdx+1 << ", name: '" << desc.name << "'");
				break;
			}
			propIdx++;
		}
		if (stream.eol()) {
			stream.skipEOL();
		}
	} while (0);
	if (!res)
		res = res.wrap(strbld() << "Reading primary properties for object of type '" << object.objectType() << "'");
	return res;
}

SODL_result SODL_Loader::resolveDataBinding(SODL_Value &inOutVal, SODL_Value::Type expectedType, unsigned crtLine) {
	// 1. look up the data bindings map using inOutVal.bindingName as key
	// 2. check its type against expectedType
	// 3. update inOutVal's actual value to the retrieved data, and type to expectedType
	auto it = mapDataBindings_.find(inOutVal.bindingName);
	if (it == mapDataBindings_.end())
		return SODL_result::error(strbld() << "Undefined data binding '" << inOutVal.bindingName << "'", crtLine);
	if (!canConvertAssignType(it->second.first, expectedType))
		return SODL_result::error(strbld() << "Cannot assign binding '" << inOutVal.bindingName <<
			"' of type '" << SODL_Value::typeStr(it->second.first) << "' to property of type: '" << SODL_Value::typeStr(expectedType), crtLine);
	assertDbg(it->second.second != nullptr);
	switch(expectedType) {
	case SODL_Value::Type::Coordinate: {
		FlexCoord coordVal = ValueConverter<FlexCoord>::convertValue(it->second.first, it->second.second);
		inOutVal.numberVal = coordVal.get(FlexCoord::X_LEFT, glm::vec2(100, 100));
		inOutVal.isPercentCoord = coordVal.unit() == FlexCoord::PERCENT;
	} break;
	case SODL_Value::Type::Number:
		inOutVal.numberVal = ValueConverter<float>::convertValue(it->second.first, it->second.second);
		break;
	case SODL_Value::Type::String:
		inOutVal.stringVal = ValueConverter<std::string>::convertValue(it->second.first, it->second.second);
		break;
	case SODL_Value::Type::Bool:
		inOutVal.boolVal = *static_cast<bool*>(it->second.second);
	default:
		return SODL_result::error(strbld() << "Property of type '" << SODL_Value::typeStr(expectedType) << "' cannot use data binding", crtLine);
	}
	inOutVal.isBinding = false;
	return SODL_result::OK();
}

SODL_result SODL_Loader::checkCallbackArgumentsMatch(std::vector<SODL_Value::Type> argTypes, std::vector<SODL_Value::Type> expectedTypes, unsigned crtLine) {
	if (argTypes.size() != expectedTypes.size())
		return SODL_result::error("Action binding mismatch: wrong number of arguments", crtLine);
	for (unsigned i=0; i<argTypes.size(); i++) {
		if (argTypes[i] != expectedTypes[i])
			return SODL_result::error(strbld() << "Action binding mismatch: argument " << (i+1) << " type mismatch", crtLine);
	}
	return SODL_result::OK();
}

SODL_result SODL_Loader::readObjectBlock(ISODL_Object &object, SODL_Loader::ParseStream &stream) {
	assertDbg(stream.nextChar() == '{');
	stream.skipChar('{');
	SODL_result res = SODL_result::OK();
	do {
		while (res && !stream.eof() && stream.nextChar() != '}') {
			if (stream.eol()) {
				stream.skipEOL();
				continue;
			}
			bool classInstance = false;
			if (stream.nextChar() == '@') {
				classInstance = true;
				stream.skipChar('@');
				if (stream.nextIsWhitespace()) {
					return SODL_result::error("Expected identifier after @, found white space", stream.crtLine());
				}
			}
			std::string ident;
			res = stream.readIdentifier(ident);
			if (!res)
				break;
			if (classInstance) {
				std::shared_ptr<ISODL_Object> pInstanceObj;
				res = instantiateClass(ident, pInstanceObj, stream.crtLine());
				if (!res)
					break;
				if (!objectSupportsChildType(object, pInstanceObj->objectType()))
					return SODL_result::error(strbld() << "Object of type '" << object.objectType() <<
						"' does not support children of type '" << pInstanceObj->objectType() << "'", stream.crtLine());
				res = mergeObjectImpl(*pInstanceObj, stream);
				if (!res)
					break;
				if (!object.addChildObject(pInstanceObj))
					return SODL_result::error(strbld() << "Failed to add object of type '" << pInstanceObj->objectType() <<
						"' as child to '" << object.objectType() << "'", stream.crtLine());
			} else if (ident == "class") {
				res = readClass(object, stream);
				if (!res)
					break;
			} else if (stream.nextChar() == ':') {
				// this is a property
				stream.skipChar(':');
				if (stream.eol())
					return SODL_result::error("End of line while expecting property value after ':'", stream.crtLine());
				SODL_Property_Descriptor pdesc;
				res = object.describeProperty(ident, pdesc);
				if (!res) {
					res.addLineInfo(stream.crtLine());
					break;
				}
				if (pdesc.isObject) {
					assertDbg(pdesc.objectType != "");
					std::shared_ptr<ISODL_Object> pPropObj;
					res = instantiateObject(pdesc.objectType, pPropObj);
					if (!res)
						break;
					res = mergeObjectImpl(*pPropObj, stream);
					if (!res)
						break;
					res = object.setProperty(ident, pPropObj);
				} else {
					SODL_Value val;
					res = stream.readValue(pdesc.type, val);
					if (!res)
						break;
					res = assignPropertyValue(object, pdesc, val, 0, ident, stream.crtLine());
					if (!res)
						break;
				}
			} else {
				// this must be a child object
				if (!objectSupportsChildType(object, ident))
					return SODL_result::error(strbld() << "Object of type '" << object.objectType() <<
						"' does not support children of type '" << ident << "'", stream.crtLine());
				std::shared_ptr<ISODL_Object> pObj;
				res = loadObjectImpl(stream, &ident, pObj);
				if (!res)
					break;
				if (!object.addChildObject(pObj)) {
					res = SODL_result::error(strbld() << "Failed to add object of type '" << ident <<
						"' as child to '" << object.objectType() << "'", stream.crtLine());
					break;
				}
			}
		}
	} while (0);
	if (res && stream.eof())
		res = SODL_result::error("End of file while expecting '}'", stream.crtLine());
	if (!res)
		return res.wrap(strbld() << "Reading object block for object of type '" << object.objectType() << "'");
	stream.skipChar('}');
	return SODL_result::OK();
}

SODL_result SODL_Loader::readClass(ISODL_Object &object, SODL_Loader::ParseStream &stream) {
	std::string className;
	SODL_result res = SODL_result::OK();
	do {
		auto res = stream.readIdentifier(className);
		if (!res)
			break;
		std::shared_ptr<ISODL_Object> pClassObj;
		res = loadObjectImpl(stream, nullptr, pClassObj);
		if (!res)
			break;
		res = addClassDefinition(className, pClassObj, stream.crtLine());
	} while (0);
	if (!res)
		return res.wrap(strbld() << "Reading class '" << className << "'");
	else
		return SODL_result::OK();
}

bool SODL_Loader::objectSupportsChildType(ISODL_Object &object, std::string const& typeName) {
	if (object.supportsChildType(typeName))
		return true;
	// use factory.getTypeInfo() to recursively check supertypes of the typeName until no more supertype or match
	SODL_ObjectTypeDescriptor typeDesc;
	if (!factory_.getTypeInfo(typeName, typeDesc))
		return false;
	if (!typeDesc.superType.empty())
		return objectSupportsChildType(object, typeDesc.superType);
	else
		return false;
}

bool SODL_Loader::canConvertAssignType(SODL_Value::Type from, SODL_Value::Type to) {
	if (from == to)
		return true;
	else return (
	(from == SODL_Value::Type::Coordinate
		|| from == SODL_Value::Type::Number
		|| from == SODL_Value::Type::String)
	&& (to == SODL_Value::Type::Coordinate
		|| to == SODL_Value::Type::Number
		|| to == SODL_Value::Type::String));
}


SODL_result SODL_Loader::addClassDefinition(std::string const& className, std::shared_ptr<ISODL_Object> pClassObj, unsigned crtLine) {
	if (mapClasses_.find(className) != mapClasses_.end())
		return SODL_result::error(strbld() << "Class '" << className << "' has already been defined!", crtLine);
	mapClasses_[className] = pClassObj;
	return SODL_result::OK();
}

SODL_result SODL_Loader::instantiateClass(std::string const& className, std::shared_ptr<ISODL_Object> &out_pInstance, unsigned crtLine) {
	if (mapClasses_.find(className) == mapClasses_.end())
		return SODL_result::error(strbld() << "Undefined class '" << className << "' !", crtLine);
	out_pInstance = mapClasses_[className]->clone();
	return SODL_result::OK();
}
