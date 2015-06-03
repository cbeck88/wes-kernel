#pragma once

#include <string>
#include <utility>
#include <vector>
#include <boost/variant/recursive_variant.hpp>
#include <boost/foreach.hpp>

namespace wml {

typedef std::string Str;
typedef std::pair<Str, Str> Pair;

///////////////////////////////////////////////////////////////////////////
//  Our WML tree representation
///////////////////////////////////////////////////////////////////////////
struct body;

typedef boost::variant<boost::recursive_wrapper<body>, Pair> node;

struct body {
	Str name;                   // tag name
	std::vector<node> children; // children
};

typedef std::vector<node> config;

///////////////////////////////////////////////////////////////////////////
//  Print out the wml tree
///////////////////////////////////////////////////////////////////////////
int const tabsize = 4;

void tab(int indent) {
	for (int i = 0; i < indent; ++i)
		std::cout << ' ';
}

struct body_printer {
	body_printer(int indent = 0) : indent(indent) {}

	void operator()(body const&) const;

	int indent;
};

struct node_printer : boost::static_visitor<> {
	node_printer(int indent = 0) : indent(indent) {}

	void operator()(body const& w) const { body_printer(indent + tabsize)(w); }

	void operator()(Str const& text) const {
		tab(indent + tabsize);
		std::cout << "text: \"" << text << '"' << std::endl;
	}

	void operator()(Pair const& p) const {
		tab(indent + tabsize);
		std::cout << p.first << ": \"" << p.second << "\"" << std::endl;
	}

	int indent;
};

void body_printer::operator()(body const& w) const {
	tab(indent);
	std::cout << "tag: \"" << w.name << "\"" << std::endl;
	tab(indent);
	std::cout << '{' << std::endl;

	BOOST_FOREACH (node const& n, w.children) { boost::apply_visitor(node_printer(indent), n); }

	tab(indent);
	std::cout << '}' << std::endl;
}

struct config_printer {
	config_printer(int indent = 0) : indent(indent) {}

	void operator()(config const&) const;

	int indent;
};

void config_printer::operator()(config const& c) const {
	tab(indent);
	std::cout << '{' << std::endl;

	BOOST_FOREACH (node const& n, c) { boost::apply_visitor(node_printer(indent), n); }

	tab(indent);
	std::cout << '}' << std::endl;
}

} // end namespace wml
