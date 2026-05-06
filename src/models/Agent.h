#pragma once

#include <string>
#include <vector>
#include <map>

struct Agent {
    std::string id;
    std::string name;
    std::string description;
    std::string command;
    std::string args;
    std::string model;
    int timeout = 300;
    std::string auto_approve = "never";
    bool enabled = true;
    std::vector<std::string> tags;
    std::string instructions;
    std::map<std::string, std::string> env;
    std::vector<std::string> skills;
};
