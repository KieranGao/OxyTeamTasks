#include "ConfigManager.h"
#include <bitset>
ConfigManager::ConfigManager() {
    boost::filesystem::path current_path = boost::filesystem::current_path();
    boost::filesystem::path config_path = current_path / "config.ini";
    std::cout << "Config path: " << config_path << std::endl;

    try {
        boost::property_tree::ptree pt;
        boost::property_tree::read_ini(config_path.string(), pt);

        for (const auto &section_entry : pt) {
            const std::string &section_name = section_entry.first;
            const auto &section_tree = section_entry.second;
            SectionInfo section_info;

            for (const auto &kv : section_tree) {
                section_info._section_datas[kv.first] = kv.second.get_value<std::string>();
            }
            conf_map_[section_name] = section_info;
        }

        std::cout << "===== Parsed config =====" << std::endl;
        for (const auto &entry : conf_map_) {
            std::cout << "[" << entry.first << "]" << std::endl;
            for (auto& kv : entry.second._section_datas) {
                std::cout << "  " << kv.first << "=" << kv.second << std::endl;
            }
        }
        
    } catch (const std::exception &e) {
        std::cerr << "Config error: " << e.what() << std::endl;
    }
}
