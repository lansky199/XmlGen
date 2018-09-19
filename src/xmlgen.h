#ifndef _XMLGEN_H_
#define _XMLGEN_H_

#include <string>
#include "pugixml.hpp"

enum TypeEnum {
	TypeEnum_int = 0,	//整型
	TypeEnum_int32,
	TypeEnum_int64,
	TypeEnum_uint,
	TypeEnum_uint32,
	TypeEnum_uint64,
	TypeEnum_float,		//浮点数
	TypeEnum_double,
	TypeEnum_bool,		//boolean
	TypeEnum_string,	//字符串
	TypeEnum_max,
};
const static char* gTypeDef[TypeEnum_max] = { "int", "int32", "int64", "uint", "uint32", "uint64", "float", "double", "bool", "string" }; 
const static char* gTypeTrans[TypeEnum_max] = { "as_int", "as_int", "as_llong", "as_uint", "as_uint", "as_ullong", "as_float", "as_double", "as_bool", "as_string" };

class XmlGen {
public:
	explicit XmlGen();
	~XmlGen();

	bool generate(const std::string& fname);
private:
	void exportXml(const pugi::xml_document& doc);
	void exportHeader(const pugi::xml_document& doc);
	void exportSource(const pugi::xml_document& doc);
	void exportLua(const pugi::xml_document& doc);
private:
	void appendTypes(pugi::xml_node& xmlNode, const pugi::xml_node& node, const std::string& type);
private:
	std::string m_file;
};

#endif
