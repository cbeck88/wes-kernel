///
// A WML parser using boost spirit2.
// Based heavily on http://www.boost.org/doc/libs/1_47_0/libs/spirit/example/qi/mini_xml2.cpp
///

#define BOOST_SPIRIT_USE_PHOENIX_V3

#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_fusion.hpp>
#include <boost/spirit/include/phoenix_stl.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/variant/recursive_variant.hpp>
#include <boost/foreach.hpp>

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

namespace wml
{
	namespace fusion = boost::fusion;
	namespace phoenix = boost::phoenix;
	namespace qi = boost::spirit::qi;
	namespace ascii = boost::spirit::ascii;

	typedef std::string Str;

	///////////////////////////////////////////////////////////////////////////
	//  Our WML tree representation
	///////////////////////////////////////////////////////////////////////////
	struct body;

	typedef boost::variant<boost::recursive_wrapper<body>, Str>
		node;

	struct body
	{
		Str name;	   // tag name
		std::vector<node> children; // children
	};
}

// We need to tell fusion about our mini_xml struct
// to make it a first-class fusion citizen
BOOST_FUSION_ADAPT_STRUCT(
	wml::body,
	(wml::Str, name)(std::vector<wml::node>, children))

namespace wml
{
	///////////////////////////////////////////////////////////////////////////
	//  Print out the mini xml tree
	///////////////////////////////////////////////////////////////////////////
	int const tabsize = 4;

	void tab(int indent)
	{
		for(int i = 0; i < indent; ++i)
			std::cout << ' ';
	}

	struct body_printer
	{
		body_printer(int indent = 0)
		    : indent(indent)
		{
		}

		void operator()(body const&) const;

		int indent;
	};

	struct node_printer : boost::static_visitor<>
	{
		node_printer(int indent = 0)
		    : indent(indent)
		{
		}

		void operator()(body const& w) const
		{
			body_printer(indent + tabsize)(w);
		}

		void operator()(Str const& text) const
		{
			tab(indent + tabsize);
			std::cout << "text: \"" << text << '"' << std::endl;
		}

		int indent;
	};

	void body_printer::operator()(body const& w) const
	{
		tab(indent);
		std::cout << "tag: '" << w.name << "'" << std::endl;
		tab(indent);
		std::cout << '{' << std::endl;

		BOOST_FOREACH(node const& n, w.children) {
			boost::apply_visitor(node_printer(indent), n);
		}

		tab(indent);
		std::cout << '}' << std::endl;
	}

	///////////////////////////////////////////////////////////////////////////
	//  Our mini XML grammar definition
	///////////////////////////////////////////////////////////////////////////
	//[tutorial_xml2_grammar
	template <typename Iterator>
	struct wml_grammar
		: qi::grammar<Iterator, body(), qi::locals<Str>, ascii::space_type>
	{
		wml_grammar()
		    : wml_grammar::base_type(wml)
		{
			using qi::lit;
			using qi::lexeme;
			using ascii::char_;
			using ascii::string;
			using namespace qi::labels;

			text %= lexeme[+(char_ - '[')];
			node %= wml | text;

			start_tag %= '['
				     >> !lit('/')
				     >> lexeme[+(char_ - ']')]
				     >> ']';

			end_tag = "[/"
				  >> string(_r1)
				  >> ']';

			wml %= start_tag[_a = _1]
			       >> *node
			       >> end_tag(_a);
		}

		qi::rule<Iterator, wml::body(), qi::locals<Str>, ascii::space_type> wml;
		qi::rule<Iterator, wml::node(), ascii::space_type> node;
		qi::rule<Iterator, Str(), ascii::space_type> text;
		qi::rule<Iterator, Str(), ascii::space_type> start_tag;
		qi::rule<Iterator, void(Str), ascii::space_type> end_tag;
	};
	//]
}

///////////////////////////////////////////////////////////////////////////////
//  Main program
///////////////////////////////////////////////////////////////////////////////
namespace wml
{

	bool parse(const std::string& storage)
	{
		typedef wml_grammar<std::string::const_iterator> my_grammar;
		my_grammar xml; // Our grammar
		wml::body ast;  // Our tree

		using boost::spirit::ascii::space;
		std::string::const_iterator iter = storage.begin();
		std::string::const_iterator end = storage.end();
		bool r = phrase_parse(iter, end, xml, space, ast);

		if(r && iter == end) {
			std::cout << "-------------------------\n";
			std::cout << "Parsing succeeded\n";
			std::cout << "-------------------------\n";
			body_printer printer;
			printer(ast);
			return true;
		} else {
			std::string::const_iterator some = iter + 30;
			std::string context(iter, (some > end) ? end : some);
			std::cout << "-------------------------\n";
			std::cout << "Parsing failed\n";
			std::cout << "stopped at: \": " << context << "...\"\n";
			std::cout << "-------------------------\n";
			return false;
		}
	}
}
