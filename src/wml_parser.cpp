///
// A WML parser using boost spirit2.
// Based heavily on http://www.boost.org/doc/libs/1_47_0/libs/spirit/example/qi/mini_xml2.cpp
///

#define BOOST_SPIRIT_USE_PHOENIX_V3
#define BOOST_SPIRIT_UNICODE

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
		std::cout << "tag: \"" << w.name << "\"" << std::endl;
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
	struct whitespace {
		qi::rule<Iterator> weak, endl, all;

		whitespace() {
			weak = boost::spirit::unicode::char_(" \t\r");
			endl = boost::spirit::unicode::char_("\n");
			all = weak | endl;
		}
	};

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
			using boost::spirit::unicode::char_;
			using boost::spirit::unicode::string;
			using namespace qi::labels;

			using phoenix::construct;
			using phoenix::val;

			ws = whitespace<Iterator>();

			pair = *ws.weak >> key >> *ws.weak > lit('=') >> *ws.weak > value >> *ws.weak >> ws.endl;
			key = char_("a-zA-Z_") >> *char_("a-zA-Z_0-9");
			value = -(lit('_') >> *ws.weak) >> (angle_quoted_string | double_quoted_string | endl_terminated_string);

			angle_quoted_string = qi::string("<<") >> +(char_ - ">>") >> qi::string(">>");
			double_quoted_string = '"' >> +(char_ - '"') >> '"';
			endl_terminated_string = *(char_ - '\n');

			node = wml | pair;

			start_tag %= lit('[')
				     >> !lit('/')
				     >> lexeme[+(char_ - ']')]
				     >> lit(']');

			end_tag = lit("[/")
				  > string(_r1)
				  > lit(']');

			wml %= start_tag[_a = _1]
			       >> *node
			       > end_tag(_a);

			wml.name("wml");
			node.name("node");
			start_tag.name("start_tag");
			end_tag.name("end_tag");
			key.name("attribute_key");
			value.name("attribute_value");
			pair.name("attribute");
			angle_quoted_string.name("angle-string");
			double_quoted_string.name("quot-string");
			endl_terminated_string.name("endl-string");

			on_error<fail>(
				key, std::cerr << val("Error! Expecting ") << qi::_4			     // what failed?
					       << val(" here: \"") << construct<std::string>(qi::_3, qi::_2) // iterators to error-pos, end

					       << val("\"") << std::endl);

			on_error<fail>(
				value, std::cerr << val("Error! Expecting ") << qi::_4			     // what failed?
					       << val(" here: \"") << construct<std::string>(qi::_3, qi::_2) // iterators to error-pos, end
					       << val("\"") << std::endl);


			on_error<fail>(
				pair, std::cerr << val("Error! Expecting ") << qi::_4			     // what failed?
					       << val(" here: \"") << construct<std::string>(qi::_3, qi::_2) // iterators to error-pos, end
					       << val("\"") << std::endl);

			on_error<fail>(
				start_tag, std::cerr << val("Error! Expecting ") << qi::_4			     // what failed?
					       << val(" here: \"") << construct<std::string>(qi::_3, qi::_2) // iterators to error-pos, end
					       << val("\"") << std::endl);

			on_error<fail>(
				end_tag, std::cerr << val("Error! Expecting ") << qi::_4			     // what failed?
					       << val(" here: \"") << construct<std::string>(qi::_3, qi::_2) // iterators to error-pos, end
					       << val("\"") << std::endl);

			on_error<fail>(
				wml, std::cerr << val("Error! Expecting ") << qi::_4			     // what failed?
					       << val(" here: \"") << construct<std::string>(qi::_3, qi::_2) // iterators to error-pos, end
					       << val("\"") << std::endl);

      BOOST_SPIRIT_DEBUG_NODE(wml);
      BOOST_SPIRIT_DEBUG_NODE(node);
      BOOST_SPIRIT_DEBUG_NODE(start_tag);
      BOOST_SPIRIT_DEBUG_NODE(end_tag);
      BOOST_SPIRIT_DEBUG_NODE(pair);
      BOOST_SPIRIT_DEBUG_NODE(key);
      BOOST_SPIRIT_DEBUG_NODE(value);
      BOOST_SPIRIT_DEBUG_NODE(double_quoted_string);
      BOOST_SPIRIT_DEBUG_NODE(angle_quoted_string);
      BOOST_SPIRIT_DEBUG_NODE(endl_terminated_string);

		}

		struct whitespace<Iterator> ws;
		qi::rule<Iterator, wml::body(), qi::locals<Str>, qi::space_type> wml;
		qi::rule<Iterator, wml::node(), qi::space_type> node;
		qi::rule<Iterator, Str(), qi::space_type> start_tag;
		qi::rule<Iterator, void(Str), qi::space_type> end_tag;
		qi::rule<Iterator, Pair()> pair;
		qi::rule<Iterator, Str()> key;
		qi::rule<Iterator, Str()> value;
		qi::rule<Iterator, Str()> double_quoted_string;
		qi::rule<Iterator, Str()> angle_quoted_string;
		qi::rule<Iterator, Str()> endl_terminated_string;

	};
}

////
// Test Cases
////

namespace wml {

template<typename T>
bool test_case(const char * str, T & gram, bool expected = true)
{
	static bool always_show = false;

	using boost::spirit::qi::space;
	std::string storage = str;
	if (storage.at(storage.size()-1) != '\n') {
		storage += "\n"; //terminate with endl
	}
	std::string::const_iterator iter = storage.begin();
	std::string::const_iterator end = storage.end();

	std::stringstream foo;

			foo << "-------------------------\n";
			foo << "Test case:\n";
			foo << str << std::endl;
			foo << "-------------------------\n";

	bool r = phrase_parse(iter, end, gram, space);

		if(r && iter == end) {
			foo << "-------------------------\n";
			foo << "Parsing succeeded\n";
			foo << "-------------------------\n";
			if (always_show || true != expected) std::cout << foo.str() << std::endl;
			return true == expected;
		} else {
			std::string::const_iterator some = iter + 80;
			std::string context(iter, (some > end) ? end : some);
			foo << "-------------------------\n";
			foo << "Parsing failed\n";
			foo << "stopped at: \": " << context << "...\"\n";
			foo << "-------------------------\n";
			if (always_show || false != expected) std::cout << foo.str() << std::endl;
			return false == expected;
		}

}

}

///////////////////////////////////////////////////////////////////////////////
//  Main program
///////////////////////////////////////////////////////////////////////////////
namespace wml
{

	bool parse(const std::string& sto)
	{
		std::string storage = sto;
		if (storage.at(storage.size()-1) != '\n') {
			storage += "\n"; //terminate with endl
		}

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


	bool parse_attr(const std::string& sto)
	{
		std::string storage = sto;
		if (sto.at(sto.size()-1) != '\n') {
			storage += "\n"; //terminate with endl
		}

		typedef wml_grammar<std::string::const_iterator> my_grammar;
		my_grammar grammar;
		auto gram = grammar.pair;
		wml::Pair ast;  // Our tree

		using boost::spirit::qi::space;
		std::string::const_iterator iter = storage.begin();
		std::string::const_iterator end = storage.end();
		bool r = phrase_parse(iter, end, gram, space, ast);

		if(r && iter == end) {
			std::cout << "-------------------------\n";
			std::cout << "Parsing succeeded\n";
			std::cout << "-------------------------\n";
			std::cout << "first: '" << ast.first << "'\n";
			std::cout << "second: '" << ast.second << "'\n";
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

	bool strip_preprocessor(std::string & input)
	{
		std::stringstream ss;
		ss.str(input);

		std::string output;

		bool in_define = false;

		int line = 1;
		std::string line_buffer;
		int define_line = 0;
		int brace_depth = 0;
		char c;

		while(ss) {
			ss.get(c);

			switch(c) {
				case '#': {
					std::string temp;
					getline(ss, temp);
					if (temp.size() >= 6) {
						if (temp.substr(0,6) == "define") {
							std::cerr << "DEBUG: got a line with a #define, number '" << line << "':\n'" << temp << "'\n";
							if (in_define) {
								std::cerr << "Found #define inside of #define\n";
								std::cerr << "Earlier define was at line " << define_line << "\n";
								std::cerr << "***\n" << output << "\n";
								return false;
							}
							in_define = true;
							define_line = line;
						} else if (temp.substr(0,6) == "enddef") {
							if (!in_define) {
								std::cerr << "Found #enddef outside of #define\n";
								std::cerr << "***\n" << output << "\n";
								return false;
							}
							in_define = false;
						}
					}
					break;
				}
				case '{': {
					brace_depth++;
					break;
				}
				case '}': {
					if (brace_depth <= 0) {
						std::cerr << "Found unexpected '{'\n";
						std::cerr << "***\n" << output << "\n";
						return false;
					}
					brace_depth--;
					break;
				}
				default: {
					if (!in_define && brace_depth == 0) {
						output += c;
						line_buffer += c;
					}
					
					break;
				}
			}
			if (c == '\n') {
				++line;
				line_buffer = "";
			}
		}

		std::cout << "*** Finished stripping preprocessing: ***" << std::endl;
		std::cout << input << std::endl;
		std::cout << "----->" << std::endl;
		std::cout << output << std::endl;
		std::cout << "***\n" << std::endl;

		input = output;

		return true;
	}


	void test()
	{

		typedef wml::wml_grammar<std::string::const_iterator> my_grammar;
		my_grammar gram; // Our grammar
		wml::body ast;  // Our tree

		auto pair_gram = gram.pair;
		

		wml::test_case("a=b",pair_gram);
		wml::test_case("a23=b43",pair_gram);
		wml::test_case("a=",pair_gram);
		wml::test_case("a-asdf=23432",pair_gram, false);
		wml::test_case("a_asdf=23432",pair_gram);
		wml::test_case("a=\"\nfoooooooo\"",pair_gram);
		wml::test_case("a=<<asdf>>",pair_gram);

		wml::test_case("[foo][/foo]", gram);
		wml::test_case("[foo][bar][/bar][/foo][baz][/baz]", gram, false);
		wml::test_case("\
[foo]\n\
  a=b\n\
  [bar]\n\
    c=d\n\
  [/bar]\n\
[/foo]\n\
[baz]\n\
[/baz]", gram, false);

		wml::test_case("[foo]\n\
a = bde4_@342\n\
[bar]\n\
[foo]\n\
[sd]\n\
a= b\n\
[/sd]\n\
[/foo]\n\
[/bar]\n\
[/foo]\n\
", gram);

		wml::test_case("[foo]\na=\n[/foo]", gram);

		auto node_gram = gram.pair;

		wml::test_case("a=\n", node_gram);


		wml::test_case("[foo]a=b\n[/foo]", gram);

	}

}



