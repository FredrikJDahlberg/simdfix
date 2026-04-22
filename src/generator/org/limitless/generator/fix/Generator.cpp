//
// Created by Fredrik Dahlberg on 2026-04-22.
//
#include "pugixml.hpp"
#include <iostream>

int main(int argc, char** argv)
{
    pugi::xml_document doc;

    if (const auto result = doc.load_file(argv[1]); !result)
    {
        std::cerr << "XML error: " << result.description() << std::endl;
        return 1;
    }

    pugi::xml_node protocol = doc.child("protocol");
    for (pugi::xml_node message : protocol.children("message")) {
        const char* name = message.attribute("name").value();
        const char* id = message.attribute("id").value();
        std::cout << "Message: " << name << " (ID: " << id << ")\n";
    }

    pugi::xpath_node login = protocol.select_node("message[@name='Login']");
    if (login) {
        std::cout << "Found Login message via XPath!\n";
    }
    return 0;
}

