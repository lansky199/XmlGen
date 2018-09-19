#include "xmlgen.h"
#include <fstream>
#include <locale>
#include <codecvt>
#include <vector>

std::string strip(const std::string& fname, const std::string& strip) {
	return fname.substr(0, fname.size() - strip.size());
}

static void Lower(std::string* s) {
	auto end = s->end();
	for (auto it = s->begin(); it != end; ++it) {
		if ('A' <= *it && *it <= 'Z') {
			*it += 'a' - 'A';
		}
	}
}
static void Upper(std::string* s) {
	auto end = s->end();
	for (auto it = s->begin(); it != end; ++it) {
		if ('a' <= *it && *it <= 'z') {
			*it += 'A' - 'a';
		}
	}
}
static std::string toLower(const std::string& s) {
	std::string out = s;
	Lower(&out);
	return std::move(out);
}
static std::string toUpper(const std::string& s) {
	std::string out = s;
	Upper(&out);
	return std::move(out);
}

bool hasAttr(const pugi::xml_node& node, const std::string& name) {
	auto attr = node.attribute(name.c_str());
	return !attr.empty();
}

TypeEnum getType(const std::string& type) {
	auto low = toLower(type);
	for (int i = TypeEnum_int; i < TypeEnum_max; ++i) {
		if (low == gTypeDef[i]) {
			return static_cast<TypeEnum>(i);
		}
	}
	return TypeEnum_max;
}

void wstrToUtf8Real(std::string& dest, const std::wstring& src) {
	dest.clear();
	for (size_t i = 0; i < src.size(); i++) {
		wchar_t w = src[i];
		if (w <= 0x7f)
			dest.push_back((char)w);
		else if (w <= 0x7ff) {
			dest.push_back(0xc0 | ((w >> 6) & 0x1f));
			dest.push_back(0x80 | (w & 0x3f));
		}
		else if (w <= 0xffff) {
			dest.push_back(0xe0 | ((w >> 12) & 0x0f));
			dest.push_back(0x80 | ((w >> 6) & 0x3f));
			dest.push_back(0x80 | (w & 0x3f));
		}
		else if (w <= 0x10ffff) {
			dest.push_back(0xf0 | ((w >> 18) & 0x07));
			dest.push_back(0x80 | ((w >> 12) & 0x3f));
			dest.push_back(0x80 | ((w >> 6) & 0x3f));
			dest.push_back(0x80 | (w & 0x3f));
		}
		else
			dest.push_back(' ');
	}
}
void utf8toWStrReal(std::wstring& dest, const std::string& src) {
	dest.clear();
	wchar_t w = 0;
	int bytes = 0;
	wchar_t err = L' ';
	for (size_t i = 0; i < src.size(); i++) {
		unsigned char c = (unsigned char)src[i];
		if (c <= 0x7f) {//first byte
			if (bytes) {
				dest.push_back(err);
				bytes = 0;
			}
			dest.push_back((wchar_t)c);
		}
		else if (c <= 0xbf) {//second/third/etc byte
			if (bytes) {
				w = ((w << 6) | (c & 0x3f));
				bytes--;
				if (bytes == 0)
					dest.push_back(w);
			}
			else
				dest.push_back(err);
		}
		else if (c <= 0xdf) {//2byte sequence start
			bytes = 1;
			w = c & 0x1f;
		}
		else if (c <= 0xef) {//3byte sequence start
			bytes = 2;
			w = c & 0x0f;
		}
		else if (c <= 0xf7) {//3byte sequence start
			bytes = 3;
			w = c & 0x07;
		}
		else {
			dest.push_back(err);
			bytes = 0;
		}
	}
	if (bytes)
		dest.push_back(err);
}

std::string WD2CH(const wchar_t* dest) {
	std::wstring wstr;
	if (dest) {
		wstr = dest;
	}
	std::string str;
	wstrToUtf8Real(str, wstr);
	return str;
}
std::wstring CH2WD(const std::string& dest) {
	std::wstring str;
	utf8toWStrReal(str, dest);
	return str;
}


std::string utf82Gbk(const std::string& uft8str) {
	std::wstring wTemp = CH2WD(uft8str);
#ifdef _MSC_VER
	std::locale loc("zh-CN");
#else
	std::locale loc("zh_CN.GB18030");
#endif
	const wchar_t* pwszNext = nullptr;
	char* pszNext = nullptr;
	mbstate_t state = {};

	std::vector<char> buff(wTemp.size() * 2);
	int res = std::use_facet<std::codecvt<wchar_t, char, mbstate_t> >
		(loc).out(state,
			wTemp.data(), wTemp.data() + wTemp.size(), pwszNext,
			buff.data(), buff.data() + buff.size(), pszNext);

	if (std::codecvt_base::ok == res) {
		return std::string(buff.data(), pszNext);
	}
	return "";
}

std::string gbk2Utf8(std::string const &strGb2312) {
	std::vector<wchar_t> buff(strGb2312.size());
#ifdef _MSC_VER
	std::locale loc("zh-CN");
#else
	std::locale loc("zh_CN.GB18030");
#endif
	wchar_t* pwszNext = nullptr;
	const char* pszNext = nullptr;
	mbstate_t state = {};
	int res = std::use_facet<std::codecvt<wchar_t, char, mbstate_t> >
		(loc).in(state,
			strGb2312.data(), strGb2312.data() + strGb2312.size(), pszNext,
			buff.data(), buff.data() + buff.size(), pwszNext);

	if (std::codecvt_base::ok == res) {
		return WD2CH(std::wstring(buff.data(), pwszNext).c_str());
	}

	return "";
}

XmlGen::XmlGen() {

}

XmlGen::~XmlGen() {

}

bool XmlGen::generate(const std::string& fname) {
	m_file = fname;
	pugi::xml_document doc;
	pugi::xml_parse_result ret = doc.load_file(fname.c_str());
	if (!ret) { return false; }

	exportXml(doc);
	exportHeader(doc);
	exportSource(doc);
	exportLua(doc);

	return true;
}

void XmlGen::exportXml(const pugi::xml_document& doc) {
	std::string baseName = strip(m_file, ".xml");
	std::string outfile = toLower(baseName) + "Config.xml";

	pugi::xml_document xmlDoc;
	//ÉùÃ÷
	pugi::xml_node pre = xmlDoc.prepend_child(pugi::node_declaration);
	pre.append_attribute("version") = "1.0";
	pre.append_attribute("encoding") = "utf-8";

	for (pugi::xml_node node = doc.first_child(); node; node = node.next_sibling()) {
		std::string nodeName = node.name();
		pugi::xml_node nodeRoot = xmlDoc.append_child(nodeName.c_str());

		pugi::xml_node vars = node.child("vars");
		if (vars) {
			for (auto varNode : vars.children("var")) {
				auto varType = varNode.attribute("type").value();
				auto varName = varNode.attribute("name").value();
				auto type = getType(varType);
				pugi::xml_node vnode = nodeRoot.append_child(varName);
				if (type < TypeEnum_max) {
					vnode.append_attribute(varName).set_value(varType);
				}
				else {
					pugi::xml_node types = node.child("types");
					appendTypes(vnode, types, varType);
				}
			}
		}
	}
	xmlDoc.save_file(outfile.c_str(), "\t", 1U, pugi::encoding_utf8);
}

void XmlGen::exportHeader(const pugi::xml_document& doc) {
	std::string baseName = strip(m_file, ".xml");
	std::string outfile = toLower(baseName) + "Config.h";

	std::ofstream out(outfile.c_str(), std::ios::out | std::ios::binary);

	// def opener
	out << "// Generate by the xml generator compiler. DO NOT EDIT!!!\n"
		<< "// source: " << m_file << "\n"
		<< "\n"
		<< "#ifndef _XMLGEN_" << toUpper(baseName) << "_H_\n"
		<< "#define _XMLGEN_" << toUpper(baseName) << "_H_\n"
		<< "\n"
		<< "#include <string>\n"
		<< "#include <vector>\n"
		<< "#include \"pugixml.hpp\"\n"
		<< "using namespace std;\n"
		<< "\n";

	for (pugi::xml_node node = doc.first_child(); node; node = node.next_sibling()) {
		std::string nodeName = node.name();
		// class opener
		out << "class " << nodeName << " {\n";

		// load method
		out << "public:\n"
			<< "    " << nodeName << "();\n"
			<< "    ~" << nodeName << "();\n"
			<< "\n"
			<< "    bool loadConfigData(const std::string& fname);\n";

		// typedefs
		pugi::xml_node types = node.child("types");
		if (types) {
			out << "private:\n";
			for (auto typeNode : types.children("type")) {
				std::string typeName = typeNode.attribute("name").value();
				out << "    struct " << typeName << " {\n";
				for (auto itemNode : typeNode.children("item")) {
					bool multi = itemNode.attribute("multi").as_bool();
					std::string itemType = itemNode.attribute("type").value();
					std::string itemName = itemNode.attribute("name").value();
					std::string itemDes = itemNode.attribute("des").value();
					if (multi) {
						out << "        vector<" << itemType << "> ";
					}
					else {
						out << "        " << itemType << " ";
					}
					out << itemName << ";";
					if (!itemDes.empty()) {
						out << "//" << utf82Gbk(itemDes);
					}
					out << "\n";
				}
				out << "    };\n";
			}

			out << "private:\n";
			for (auto typeNode : types.children("type")) {
				std::string typeName = typeNode.attribute("name").value();
				out << "    bool load_" << typeName << "(" << typeName << "& item, const pugi::xml_node& node);\n";
			}
		}

		// vars
		pugi::xml_node vars = node.child("vars");
		if (vars) {
			out << "public:\n";
			for (auto varNode : vars.children("var")) {
				std::string varName = varNode.attribute("name").value();
				std::string varType = varNode.attribute("type").value();
				std::string varDes = varNode.attribute("des").value();
				out << "    " << varType << " " << varName << ";";
				if (!varDes.empty()) {
					out << "//" << utf82Gbk(varDes);
				}
				out << "\n";
			}
		}

		//class closer
		out << "};\n"
			<< "\n";
	}


	//def closer
	out << "#endif // _XMLGEN_" << toUpper(baseName) << "_H_\n";
	out.close();
}

void XmlGen::exportSource(const pugi::xml_document& doc) {
	std::string baseName = strip(m_file, ".xml");
	std::string outfile = toLower(baseName) + "Config.cpp";

	std::ofstream out(outfile.c_str(), std::ios::out | std::ios::binary);

	out << "// Generate by the xml generator compiler. DO NOT EDIT!!!\n"
		<< "// source: " << m_file << "\n"
		<< "\n"
		<< "#include \"" << toLower(baseName) << "Config.h\"\n"
		<< "\n";

	for (pugi::xml_node node = doc.first_child(); node; node = node.next_sibling()) {
		std::string nodeName = node.name();
		out << nodeName << "::" << nodeName << "() {\n"
			<< "}\n"
			<< "\n"
			<< nodeName << "::" << "~" << nodeName << "() {\n"
			<< "}\n"
			<< "\n";

		pugi::xml_node types = node.child("types");
		if (types) {
			for (auto typeNode : types.children("type")) {
				std::string typeName = typeNode.attribute("name").value();
				out << "bool " << nodeName << "::load_" << typeName << "(" << typeName << "& item, const pugi::xml_node& node) {\n";
				for (auto itemNode : typeNode.children("item")) {
					std::string itemName = itemNode.attribute("name").value();
					std::string itemType = itemNode.attribute("type").value();
					bool multi = itemNode.attribute("multi").as_bool();
					auto type = getType(itemType);
					if (multi) {
						out << "    for (auto itemNode : node.children(\"" << itemName << "\")) {\n";
						if (type < TypeEnum_max) {
							out << "        " << itemType << " item_ = itemNode.attribute(\"" << itemName << "\")." << gTypeTrans[type] << "();\n";
						}
						else {
							out << "        " << itemType << " item_;\n"
								<< "        if (!load_" << itemType << "(item_, itemNode)) {\n"
								<< "            return false;\n"
								<< "        }\n";
								
						}
						out << "        item." << itemName << ".push_back(item_);\n";
						out << "    }\n";
					}
					else {
						if (type < TypeEnum_max) {
							out << "    item." << itemName << " = node.attribute(\"" << itemName << "\")." << gTypeTrans[type] << "();\n";
						}
						else {
							out << "    if (!load_" << itemType << "(item." << itemName << ", node.child(\"" << itemName << "\"))) {\n"
								<< "        return false;\n"
								<< "    }\n";
						}
					}
				}
				out << "    return true;\n"
					<< "}\n"
					<< "\n";
			}
		}

		// load method
		out << "bool " << nodeName << "::loadConfigData(const std::string& fname) {\n"
			<< "    pugi::xml_document doc;\n"
			<< "    pugi::xml_parse_result ret = doc.load_file(fname.c_str());\n"
			<< "    if (!ret) {\n"
			<< "        return false;\n"
			<< "    }\n"
			<< "\n"
			<< "    pugi::xml_node root = doc.first_child();\n"
			<< "    if (!root) {\n"
			<< "        return false;\n"
			<< "    }\n"
			<< "\n";

		pugi::xml_node vars = node.child("vars");
		if (vars) {
			for (auto varNode : vars.children("var")) {
				std::string varType = varNode.attribute("type").value();
				std::string varName = varNode.attribute("name").value();
				auto type = getType(varType);
				out << "    {\n"
					<< "        pugi::xml_node node = root.child(\"" << varName << "\");\n"
					<< "        if (!node) {\n"
					<< "            return false;\n"
					<< "        }\n";
				if (type < TypeEnum_max) {
					out << "        " << varName << " = node.attribute(\"" << varName << "\")." << gTypeTrans[type] << "();\n";
				}
				else {
					out << "        if (!load_" << varNode.attribute("type").value() << "(" << varName << ", node)) {\n" 
						<< "            return false;\n"
						<< "        }\n";
				}
				out << "    }\n";
			}
		}

		out << "    return true;\n"
			<< "}\n"
			<< "\n";
	}

	out.close();
}

void XmlGen::exportLua(const pugi::xml_document& doc) {
	std::string baseName = strip(m_file, ".xml");
	std::string outfile = toLower(baseName) + "Config.lua";

	std::ofstream out(outfile.c_str(), std::ios::out | std::ios::binary);

	// def opener
	out << "-- Generate by the xml generator compiler. DO NOT EDIT!!!\n"
		<< "-- source: " << m_file << "\n"
		<< "\n"
		<< "local xmlParser = require(\"xmlSimple\")\n"
		<< "\n";

	for (pugi::xml_node node = doc.first_child(); node; node = node.next_sibling()) {
		std::string nodeName = node.name();
		out << "function load" << nodeName << "Config(file)\n"
			<< "\n";
		pugi::xml_node types = node.child("types");
		if (types) {
			for (auto typeNode : types.children("type")) {
				std::string typeName = typeNode.attribute("name").value();
				out << "    local function load_" << typeName << "(xmlNode) \n"
					<< "        local item = {}\n";
				for (auto itemNode : typeNode.children("item")) {
					std::string itemName = itemNode.attribute("name").value();
					std::string itemType = itemNode.attribute("type").value();
					bool multi = itemNode.attribute("multi").as_bool();
					auto type = getType(itemType);
					if (multi) {
						out << "        item." << itemName << " = {}\n"
							<< "        for k, v in pairs(xmlNode:children()) do\n"
							<< "            if v:name() == \"" << itemName << "\" then\n";
						if (type < TypeEnum_max) {
							if (type == TypeEnum_string || type == TypeEnum_uint64) {
								out << "                local item_ = v[\"@" << itemName << "\"]\n";
							}
							else if (type == TypeEnum_bool) {
								out << "                local item_ = false\n"
									<< "                if v[\"@" << itemName << "\"] == \"true\" then\n"
									<< "                    item_ = true\n"
									<< "                end\n";
							}
							else {
								out << "                local item_ = tonumber(v[\"@" << itemName << "\"])\n";
							}
						}
						else {
							out << "                local item_ = load_" <<  itemType << "(v)\n";

						}
						out << "                table.insert(item." << itemName << ", item_);\n"
							<< "            end\n"
							<< "        end\n";
					}
					else {
						if (type < TypeEnum_max) {
							if (type == TypeEnum_string || type == TypeEnum_uint64) {
								out << "        item." << itemName << " = xmlNode[\"@" << itemName << "\"]\n";
							}
							else if (type == TypeEnum_bool) {
								out << "        item." << itemName << " = false\n"
									<< "        if xmlNode[\"@" << itemName << "\"] == \"true\" then\n"
									<< "            item." << itemName << " = false\n"
									<< "        end\n";
							}
							else {
								out << "        item." << itemName << " = tonumber(xmlNode[\"@" << itemName << "\"])\n";
							}
						}
						else {
							out << "        item." << itemName << " = load_" << itemType << "(xmlNode." << itemName << ")\n";
						}
					}
				}
				out << "        return item\n"
					<< "    end\n"
					<< "\n";
			}
		}

		// load method
		out << "    local configData = {}\n"
			<< "	local xml = xmlParser:loadFile(file)\n"
			<< "    local xmlNode = xml." << nodeName << "\n"
			<< "\n";

		pugi::xml_node vars = node.child("vars");
		if (vars) {
			for (auto varNode : vars.children("var")) {
				std::string varType = varNode.attribute("type").value();
				std::string varName = varNode.attribute("name").value();
				auto type = getType(varType);
				if (type < TypeEnum_max) {
					if (type == TypeEnum_string || type == TypeEnum_uint64) {
						out << "    configData." << varName << " = xmlNode[\"@" << varName << "\"]\n";
					}
					else if (type == TypeEnum_bool) {
						out << "    configData." << varName << " = false\n"
							<< "    if xmlNode[\"@" << varName << "\"] == \"true\" then\n"
							<< "        configData." << varName << " = false\n"
							<< "    end\n";
					}
					else {
						out << "    configData." << varName << " = tonumber(xmlNode[\"@" << varName << "\"])\n";
					}
				}
				else {
					out << "    configData." << varName << " = load_" << varType << "(xmlNode." << varName << ")\n";
				}
			}
		}

		out << "    return configData\n"
			<< "end\n";
	}

	out.close();
}

void XmlGen::appendTypes(pugi::xml_node& xmlNode, const pugi::xml_node& node, const std::string& type) {
	pugi::xml_node typeNode = node.find_child([&type](const pugi::xml_node& node) {return type == node.attribute("name").value(); });
	if (typeNode) {
		for (auto itemNode : typeNode.children("item")) {
			auto itemName = itemNode.attribute("name").value();
			auto itemType = itemNode.attribute("type").value();
			bool multi = itemNode.attribute("multi").as_bool();
			auto type = getType(itemType);
			if (multi) {
				pugi::xml_node comment = xmlNode.append_child(pugi::node_comment);
				comment.set_value("multi");

				pugi::xml_node vnode = xmlNode.append_child(itemName);
				if (type < TypeEnum_max) {
					vnode.append_attribute(itemName).set_value(itemType);
				}
				else {
					appendTypes(vnode, node, itemType);
				}

			}
			else {
				if (type < TypeEnum_max) {
					xmlNode.append_attribute(itemName).set_value(itemType);
				}
				else {
					pugi::xml_node vnode = xmlNode.append_child(itemName);
					appendTypes(vnode, node, itemType);
				}
			}
		}
	}
}
