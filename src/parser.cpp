#include "parser.hpp"

#include <boost/property_tree/ptree.hpp>

#include <string>

using boost::property_tree::ptree;

void handle_node(const ptree& pt, int depth)
{
	for(auto itor = pt.begin(); itor != pt.end(); ++itor) {
		for(int n = 0; n != depth; ++n) { std::cerr << "  "; }
		std::cerr << "XML: " << itor->first << " -> " << itor->second.data() << "\n";
		handle_node(itor->second, depth+1);
		for(int n = 0; n != depth; ++n) { std::cerr << "  "; }
		std::cerr << "DONE: " << itor->first << "\n";
	}
}

