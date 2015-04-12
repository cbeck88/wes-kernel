///
// A WML parser using boost spirit2.
// Based heavily on http://www.boost.org/doc/libs/1_47_0/libs/spirit/example/qi/mini_xml2.cpp
///

#define BOOST_SPIRIT_USE_PHOENIX_V3

#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_object.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_fusion.hpp>
#include <boost/spirit/include/phoenix_stl.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/std_pair.hpp>
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
	typedef std::pair<Str, Str> Pair;

	///////////////////////////////////////////////////////////////////////////
	//  Our WML tree representation
	///////////////////////////////////////////////////////////////////////////
	struct body;

	typedef boost::variant<boost::recursive_wrapper<body>, Pair>
		node;

	struct body
	{
		Str name;		    // tag name
		std::vector<node> children; // children
	};
}

// We need to tell fusion about our wml struct
// to make it a first-class fusion citizen
BOOST_FUSION_ADAPT_STRUCT(
	wml::body,
	(wml::Str, name)(std::vector<wml::node>, children))

namespace wml
{
	///////////////////////////////////////////////////////////////////////////
	//  Print out the wml tree
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

		void operator()(Pair const& p) const
		{
			tab(indent + tabsize);
			std::cout << p.first << ": \"" << p.second << "\"" << std::endl;
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
	//  Our WML grammar definition
	///////////////////////////////////////////////////////////////////////////
	template <typename Iterator>
	struct wml_grammar
		: qi::grammar<Iterator, body(), qi::locals<Str>, qi::space_type>
	{
		wml_grammar()
		    : wml_grammar::base_type(wml, "wml")
		{
			using qi::lit;
			using qi::lexeme;
			using qi::on_error;
			using qi::fail;
			using ascii::char_;
			using ascii::string;
			using namespace qi::labels;

			using phoenix::construct;
			using phoenix::val;

			pair = key >> lit('=') >> value >> lit("\n");
			key = qi::char_("a-zA-Z_") >> *qi::char_("a-zA-Z_0-9");
			value %= lexeme[+(char_ - '[')];

			node %= wml | pair;

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

			wml.name("wml");
			node.name("node");
			start_tag.name("start_tag");
			end_tag.name("end_tag");
			key.name("attribute_key");
			value.name("attribute_value");
			pair.name("attribute");

			on_error<fail>(
				wml, std::cerr << val("Error! Expecting ") << qi::_4			     // what failed?
					       << val(" here: \"") << construct<std::string>(qi::_3, qi::_2) // iterators to error-pos, end
					       << val("\"") << std::endl);
		}

		qi::rule<Iterator, wml::body(), qi::locals<Str>, qi::space_type> wml;
		qi::rule<Iterator, wml::node(), qi::space_type> node;
		qi::rule<Iterator, Str(), qi::space_type> start_tag;
		qi::rule<Iterator, void(Str), qi::space_type> end_tag;
		qi::rule<Iterator, Pair(), qi::space_type> pair;
		qi::rule<Iterator, Str(), qi::space_type> key;
		qi::rule<Iterator, Str(), qi::space_type> value;
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
		my_grammar gram; // Our grammar
		wml::body ast;  // Our tree

		using boost::spirit::qi::space;
		std::string::const_iterator iter = storage.begin();
		std::string::const_iterator end = storage.end();
		bool r = phrase_parse(iter, end, gram, space, ast);

		if(r && iter == end) {
			std::cout << "-------------------------\n";
			std::cout << "Parsing succeeded\n";
			std::cout << "-------------------------\n";
			body_printer printer;
			printer(ast);
			return true;
		} else {
			std::string::const_iterator some = iter + 80;
			std::string context(iter, (some > end) ? end : some);
			std::cout << "-------------------------\n";
			std::cout << "Parsing failed\n";
			std::cout << "stopped at: \": " << context << "...\"\n";
			std::cout << "-------------------------\n";
			return false;
		}
	}
}
