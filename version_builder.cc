#include "version_builder.h"

void VersionBuilder::Add(Elf_Versym versym, std::string soname, std::string version, StrtabBuilder& strtab) {
    if (is_special_ver_ndx(versym)) {
        CHECK(soname.empty() && version.empty()) << " excess soname or version information is given.";
        LOG(INFO) << "VersionBuilder::" << special_ver_ndx_to_str(versym);

        vers.push_back(versym);
    } else if (versym == NEED_NEW_VERNUM) {
        CHECK(!soname.empty() && !version.empty());

        auto found_filename = soname_to_filename_.find(soname);
        CHECK(found_filename != soname_to_filename_.end())
            << soname << " does not exists in soname_to_filename." << SOLD_LOG_KEY(soname) << SOLD_LOG_KEY(version);
        std::string filename = found_filename->second;

        strtab.Add(filename);
        strtab.Add(version);

        if (data.find(filename) != data.end()) {
            if (data[filename].find(version) != data[filename].end()) {
                ;
            } else {
                data[filename][version] = vernum;
                vernum++;
            }
        } else {
            std::map<std::string, int> ma;
            ma[version] = vernum;
            data[filename] = ma;
            vernum++;
        }
        LOG(INFO) << "VersionBuilder::Add(" << data[filename][version] << ", " << soname << ", " << version << ")";
        vers.push_back(data[filename][version]);
    } else {
        LOG(FATAL) << "Inappropriate versym = " << versym;
    }
}

uintptr_t VersionBuilder::SizeVerneed() const {
    uintptr_t s = 0;
    for (const auto& m1 : data) {
        s += sizeof(Elf_Verneed);
        for (const auto& m2 : m1.second) {
            s += sizeof(Elf_Vernaux);
        }
    }
    return s;
}

void VersionBuilder::EmitVersym(FILE* fp) {
    for (auto v : vers) {
        CHECK(fwrite(&v, sizeof(v), 1, fp) == 1);
    }
}

void VersionBuilder::EmitVerneed(FILE* fp, StrtabBuilder& strtab) {
    int n_verneed = 0;
    for (const auto& m1 : data) {
        n_verneed++;

        Elf_Verneed v;
        v.vn_version = VER_NEED_CURRENT;
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
            a.vna_flags = VER_FLG_WEAK;
            a.vna_other = m2.second;
            a.vna_name = strtab.GetPos(m2.first);
            a.vna_next = (n_vernaux == m1.second.size()) ? 0 : sizeof(Elf_Vernaux);

            CHECK(fwrite(&a, sizeof(a), 1, fp) == 1);
        }
    }
}
