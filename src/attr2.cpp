#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/qi.hpp>

#include "wml_parser.hpp"

#include <iostream>
#include <string>
#include <vector>


////////////////////////////////////////////////////////////////////////////
//  Main program
////////////////////////////////////////////////////////////////////////////
int
main()
{
    std::cout << "/////////////////////////////////////////////////////////\n\n";
    std::cout << "\t\tAn attribute parser for Spirit...\n\n";
    std::cout << "/////////////////////////////////////////////////////////\n\n";

    std::cout << "Give me an attribute.\n";
    std::cout << "Type [q or Q] to quit\n\n";

    std::string str;
    while (getline(std::cin, str))
    {
        if (str.empty() || str[0] == 'q' || str[0] == 'Q')
            break;

        if (wml::parse_attr(str))
        {
            std::cout << "-------------------------\n";
            std::cout << "Parsing succeeded\n";
            std::cout << str << " Parses OK: " << std::endl;
        }
        else
        {
            std::cout << "-------------------------\n";
            std::cout << "Parsing failed\n";
            std::cout << "-------------------------\n";
        }
    }

    std::cout << "Bye... :-) \n\n";
    return 0;
}
