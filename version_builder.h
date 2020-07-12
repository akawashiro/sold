#pragma once

#include <map>

#include "strtab_builder.h"
#include "utils.h"

class VersionBuilder {
public:
    Elf_Versym Add(Elf_Versym ver, std::string filename, std::string version_name, StrtabBuilder& strtab) {
        if (ver == VER_NDX_LOCAL) {
            LOGF("VersionBuilder::VER_NDX_LOCAL\n");
            return ver;
        } else if (ver == VER_NDX_GLOBAL) {
            LOGF("VersionBuilder::VER_NDX_GLOBAL\n");
            return ver;
        } else {
            if (data.find(filename) != data.end()) {
                if (data[filename].find(version_name) != data[filename].end()) {
                    ;
                } else {
                    data[filename][version_name] = vernum;
                    vernum++;
                }
            } else {
                std::map<std::string, int> ma;
                ma[version_name] = vernum;
                data[filename] = ma;
                vernum++;
            }
            LOGF("VersionBuilder::Add(%d, %s, %s)\n", data[filename][version_name], filename.c_str(), version_name.c_str());
            return data[filename][version_name];
        }
    }

private:
    int vernum = 2;
    std::map<std::string, std::map<std::string, int>> data;
};
