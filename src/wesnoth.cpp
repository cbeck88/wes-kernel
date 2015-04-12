
/*
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
*/

#include "wml_parser.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <sstream>

///////////////////////////////////////////////////////////////////////////////
//  Main program
///////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
    char const* filename;
    if (argc > 1)
    {
        filename = argv[1];
    }
    else
    {
        std::cerr << "Error: No input file provided." << std::endl;
        return 1;
    }

    std::ifstream in(filename, std::ios_base::in);

    if (!in)
    {
        std::cerr << "Error: Could not open input file: "
            << filename << std::endl;
        return 1;
    }

    std::string storage; // We will read the contents here.
    in.unsetf(std::ios::skipws); // No white space skipping!
    std::copy(
        std::istream_iterator<char>(in),
        std::istream_iterator<char>(),
        std::back_inserter(storage));


	if (wml::parse(storage)) {
		std::cout << "Returning SUCCESS.\n";
		return 0;
	} else {
		std::cout << "Returning ERROR.\n";
		return 1;
	}
}

