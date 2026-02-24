#pragma once
#include "const.h"
struct SectionInfo {
	SectionInfo(){}
	~SectionInfo() {}
	SectionInfo(const SectionInfo& src) {
		_section_datas=src._section_datas;
	}
	SectionInfo& operator =(const SectionInfo& src) {//被编译器翻译成 y.operator=(x);
		if (&src == this) {
			return *this;
		}
		this->_section_datas = src._section_datas;
		return *this;
	}
	std::map<std::string, std::string> _section_datas;
	std::string operator[](const std::string& key) {//重载运算符[] 不重载只能用SectionInfo._section_dates[key]  重载后就可ui Section[key]  
		if (_section_datas.find(key) == _section_datas.end()) {
			return "";
		}
		return _section_datas[key];
	}
	std::string GetValue(const std::string& key) {
		if (_section_datas.find(key) == _section_datas.end()) {
			return "";
		}
		// 这里可以添加一些边界检查  
		return _section_datas[key];
	}
};
class ConfigMgr //不用单例了，作为全局使用就可以
{
public:
	~ConfigMgr() {
		_config_map.clear();
	}
	SectionInfo operator[](const std::string &section) {
		if (_config_map.find(section) == _config_map.end()) {
			return SectionInfo();
		}
		return _config_map[section];
	}
	static ConfigMgr& Inst() {
		static ConfigMgr cfg_mgr;//生命周期和进程生命周期同步 c++11之后 这个只初始化一次 
		return cfg_mgr;
	}
	ConfigMgr(const ConfigMgr& src) {
		_config_map = src._config_map;
	}
	ConfigMgr& operator = (const ConfigMgr& src) {
		if (&src == this) {
			return *this;
		}
		_config_map = src._config_map;
		return *this;
	}
	std::string GetValue(const std::string& section, const std::string& key);
private:
	ConfigMgr();
	std::map<std::string, SectionInfo> _config_map;

};

