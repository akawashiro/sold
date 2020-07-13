#pragma once

#include <stdio.h>

#include <map>

#include "hash.h"
#include "strtab_builder.h"
#include "utils.h"

class VersionBuilder {
public:
    void Add(Elf_Versym ver, std::string filename, std::string version_name, StrtabBuilder& strtab) {
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
            for (const auto& m2 : m1.second) {
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

    void EmitVerneed(FILE* fp, StrtabBuilder& strtab) {
        int n_verneed = 0;
        for (const auto& m1 : data) {
            n_verneed++;

            Elf_Verneed v;
            v.vn_version = 1;
            v.vn_cnt = m1.second.size();
            v.vn_file = strtab.GetPos(m1.first);
            v.vn_aux = sizeof(Elf_Verneed);
            v.vn_next = (n_verneed == data.size()) ? 0 : sizeof(Elf_Verneed) + m1.second.size() * sizeof(Elf_Vernaux);

            CHECK(fwrite(&v, sizeof(v), 1, fp) == 1);

            int n_vernaux = 0;
            for (const auto& m2 : m1.second) {
                n_vernaux++;

                Elf_Vernaux a;
                a.vna_hash = CalcHash(m2.first);
                a.vna_flags = 0;  // TODO(akawashiro) check formal document
                a.vna_other = m2.second;
                a.vna_name = strtab.GetPos(m2.first);
                a.vna_next = (n_vernaux == m1.second.size()) ? 0 : sizeof(Elf_Vernaux);

                CHECK(fwrite(&a, sizeof(a), 1, fp) == 1);
            }
        }
    }

private:
    int vernum = 2;
    std::map<std::string, std::map<std::string, int>> data;
    std::vector<Elf_Versym> vers;
};
