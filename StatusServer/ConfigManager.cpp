#include "ConfigManager.h"

ConfigManager::ConfigManager() {
    boost::filesystem::path current_path = boost::filesystem::current_path();
    boost::filesystem::path config_path = current_path / "config.ini";
    std::cerr << "Config path: " << config_path << std::endl;
    try {
        boost::property_tree::ptree pt;
        boost::property_tree::read_ini(config_path.string(), pt);
        for (const auto &section_entry : pt) {
            const std::string &section_name = section_entry.first;
            const auto &section_tree = section_entry.second;
            SectionInfo section_info;
            // 遍历 section 下所有 key-value
            for (const auto &kv : section_tree) {
                const std::string &key = kv.first;
                std::string value = kv.second.get_value<std::string>();
                section_info._section_datas[key] = value;
            }
            conf_map_[section_name] = section_info;
        }
        std::cerr << "===== Parsed config arguments =====" << std::endl;
        for (const auto &entry : conf_map_) {
            std::cerr << "[" << entry.first << "]" << std::endl;
            for (const auto &kv : entry.second._section_datas) {
                std::cerr << kv.first << " = " << kv.second << std::endl;
            }
        }

    } catch (const std::exception &e) {
        std::cerr << "Config arguments parse error!" << e.what() << std::endl;
    }
}