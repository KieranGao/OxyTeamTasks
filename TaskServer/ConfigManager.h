#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include "Global.h"
#include "Singleton.h"
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <map>
#include <string>
#include <iostream>

struct SectionInfo {
    std::map<std::string, std::string> _section_datas;

    std::string operator[](const std::string &key) const {
        auto it = _section_datas.find(key);
        return (it != _section_datas.end()) ? it->second : "";
    }
};

class ConfigManager : public Singleton<ConfigManager> {
    friend class Singleton<ConfigManager>;

public:
    const SectionInfo& operator[](const std::string &section) const {
        static SectionInfo empty;
        auto it = conf_map_.find(section);
        if (it != conf_map_.end()) {
            return it->second;
        }
        return empty;
    }

private:
    ConfigManager();
    std::map<std::string, SectionInfo> conf_map_;
};

#endif // CONFIGMANAGER_H
