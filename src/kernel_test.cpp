#include "kernel.hpp"

#include <iostream>
#include <fstream>

using std::ifstream;

int main()
{
	const std::string path = "data/kernel/init.lua";
	ifstream reader;
	reader.open(path);

	if (!reader) {
		std::cerr << "Error could not open '" << path << "'\n";
		return 1;
	}
	
	std::string contents;

	while (!reader.eof()) {
		std::string STRING;
		getline(reader, STRING);
		contents += STRING;
	}
	reader.close();

	wesnoth::kernel k(contents.begin(), contents.end());

    k.set_external_log(&std::cout);

    std::cout << "/////////////////////////////////////////////////////////\n\n";
    std::cout << "\t\tMade a kernel. Talk to it!\n\n";
    std::cout << "/////////////////////////////////////////////////////////\n\n";

    std::cout << "Give me a lua directive.\n";
    std::cout << "Type [q or Q] to quit\n\n";

	std::cout << "\n>";
	std::cout.flush();

    std::string str;
    while (getline(std::cin, str))
    {
        if (str.empty() || str[0] == 'q' || str[0] == 'Q')
            break;

	std::cout << str << std::endl;

	wesnoth::kernel::event_result result = k.execute(str);

	if (result.error) {
            std::cout << "-------------------------\n";
            std::cout << "Error: " << result.error << "\n";
            std::cout << "-------------------------\n";
	}

	std::cout << "\n>";
	std::cout.flush();

    }

    std::cout << "Bye... :-) \n\n";
    return 0;

}
