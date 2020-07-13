#pragma once

#include <map>

#include "hash.h"
#include "strtab_builder.h"
#include "utils.h"

class VersionBuilder {
public:
    void Add(Elf_Versym ver, std::string filename, std::string version_name, StrtabBuilder& strtab) {
        if (name != "") strtab.Add(name);
        if (filename != "") strtab.Add(filename);
        if (version_name != "") strtab.Add(version_name);

        if (ver == VER_NDX_LOCAL) {
            LOGF("VersionBuilder::VER_NDX_LOCAL\n");
            vers.push_back(ver);
        } else if (ver == VER_NDX_GLOBAL) {
            LOGF("VersionBuilder::VER_NDX_GLOBAL\n");
            vers.push_back(ver);
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
            vers.push_back(data[filename][version_name]);
        }
    }

    uintptr_t SizeVersym() const { return vers.size() * sizeof(Elf_Versym); }

    uintptr_t SizeVerneed() const {
        uintptr_t s = 0;
        for (const auto& m1 : data) {
            s += sizeof(Elf_Verneed);
            for (const auto& m2 : m1) {
                s += sizeof(Elf_Vernaux);
            }
        }
        return s;
    }

    int NumVerneed() const { return data.size(); }

    void EmitVersym(FILE* fp) {
        for (auto v : vers) {
            CHECK(fwrite(&v, sizeof(v), 1, fp) == 1);
        }
    }

    void EmitVerneed(File* fp, StrtabBuilder& strtab) {
        for (const auto& m1 : data) {
            Elf_Verneed v;
            v.vn_version = 1;
            v.vn_cnt = m1.size();
            v.vn_file = strtab.Add(m1.first);
            v.vn_next = m1.size() * sizeof(Elf64_Vernaux);

            CHECK(fwrite(&v, sizeof(v), 1, fp) == 1);

            for (const auto& m2 : m1) {
                Elf_Vernaux a;
                a.vna_hash = CalcHash(m2.first);
                a.vna_flags = ????;
                a.vna_other = m2.second;
                a.vna_name = strtab.Add(m2.first);
                a.vna_next = sizeof(Elf_Vernaux);
            }

            CHECK(fwrite(&a, sizeof(a), 1, fp) == 1);
        }
    }

private:
    int vernum = 2;
    std::map<std::string, std::map<std::string, int>> data;
    std::vector<Elf_Versym> vers;
};
