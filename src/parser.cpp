#include "parser.hpp"

#include <boost/property_tree/ptree.hpp>

#include <string>

void handle_node(const boost::property_tree::ptree& ptree, int depth)
{
	for(auto itor = ptree.begin(); itor != ptree.end(); ++itor) {
		for(int n = 0; n != depth; ++n) { std::cerr << "  "; }
		std::cerr << "XML: " << itor->first << " -> " << itor->second.data() << "\n";
		handle_node(itor->second, depth+1);
		for(int n = 0; n != depth; ++n) { std::cerr << "  "; }
		std::cerr << "DONE: " << itor->first << "\n";
	}
}

