#include "parser.hpp"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

using boost::property_tree::ptree;

#include <string>
#include <sstream>

int main() {
	std::string test_doc = "<root>Some text<bold attr='blah' attr2='bleh'>more text</bold>yet more text</root>";
	std::istringstream s(test_doc);
	ptree pt;
	boost::property_tree::xml_parser::read_xml(s, pt, boost::property_tree::xml_parser::no_concat_text);

	handle_node(pt, 0);
}
