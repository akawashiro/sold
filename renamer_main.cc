#include <getopt.h>

#include <fstream>
#include <map>
#include <set>

#include "elf_binary.h"
#include "strtab_builder.h"
#include "utils.h"

std::map<std::string, std::string> ReadMappingFile(std::string file) {
    std::string line;
    std::set<std::string> froms, tos;
    std::map<std::string, std::string> res;
    std::ifstream infile(file);
    while (std::getline(infile, line)) {
        std::istringstream iss(line);
        std::string from, to;
        if (!(iss >> from >> to)) {
            CHECK(false) << "Cannot parse the mapping file";
        }
        if (!froms.insert(from).second) {
            CHECK(false) << from << " is duplicated.";
        }
        if (!tos.insert(to).second) {
            CHECK(false) << to << " is duplicated.";
        }
        res[from] = to;
    }
    LOG(INFO) << res.size();
    return res;
}

void Rename(std::unique_ptr<ELFBinary> bin, std::string outfile, std::map<std::string, std::string> mapping) {
    StrtabBuilder strtab_builder(mapping);

    FILE* fp = fopen(outfile.c_str(), "wb");

    // Emit the new Ehdr
    {
        Elf_Ehdr e = *bin->ehdr();
        e.e_shstrndx = 0;
        e.e_shoff = 0;
        e.e_shnum = 0;
        e.e_phnum++;
        Write(fp, e);
    }

    // Collect all names from symbols and rename them
    std::set<int> sym_indecies = bin->CollectSymbolsFromDynamic();
    CHECK_EQ(*sym_indecies.begin(), 0);                         // Check we collect all symbols
    CHECK_EQ(*sym_indecies.rbegin() + 1, sym_indecies.size());  // Check we collect all symbols
    LOG(INFO) << SOLD_LOG_KEY(sym_indecies.size());
    std::vector<std::string> sym_names;
    for (int i : sym_indecies) {
        LOG(INFO) << SOLD_LOG_KEY(i);
        Elf_Sym* s = bin->symtab() + i;
        std::string n = bin->Str(s->st_name);
        strtab_builder.Add(n);
        if (mapping.find(n) != mapping.end()) {
            sym_names.emplace_back(mapping[n]);
        } else {
            sym_names.emplace_back(n);
        }
        s->st_name = strtab_builder.GetPos(n);
        LOG(INFO) << SOLD_LOG_KEY(n);
    }

    // Rewrite strings in dynamic
    for (const Elf_Phdr* pp : bin->phdrs()) {
        Elf_Phdr p = *pp;
        LOG(INFO) << SOLD_LOG_BITS(p.p_filesz) << SOLD_LOG_BITS(p.p_type) << SOLD_LOG_BITS(p.p_offset);

        if (p.p_type == PT_DYNAMIC) {
            CHECK_EQ(p.p_filesz % sizeof(Elf_Dyn), 0);
            LOG(INFO) << SOLD_LOG_BITS(p.p_offset);

            for (size_t i = 0; i < p.p_filesz / sizeof(Elf_Dyn); ++i) {
                Elf_Dyn* dyn = const_cast<Elf_Dyn*>(reinterpret_cast<const Elf_Dyn*>(bin->head() + p.p_offset + sizeof(Elf_Dyn) * i));
                LOG(INFO) << SOLD_LOG_BITS(dyn->d_tag) << SOLD_LOG_BITS(dyn);
                switch (dyn->d_tag) {
                    case DT_NEEDED:
                        dyn->d_un.d_val = strtab_builder.Add(bin->Str(dyn->d_un.d_val));
                        LOG(INFO) << "Rewrite name of " << ShowDynamicEntryType(dyn->d_tag);
                        break;
                    case DT_SONAME:
                        dyn->d_un.d_val = strtab_builder.Add(bin->Str(dyn->d_un.d_val));
                        LOG(INFO) << "Rewrite name of " << ShowDynamicEntryType(dyn->d_tag);
                        break;
                    default:
                        break;
                }
            }
        }
    }

    // Rewrite strings in version information
    for (int index : sym_indecies) {
        if (bin->verneed() && bin->versym() && !is_special_ver_ndx(bin->versym()[index])) {
            Elf_Verneed* vn = bin->verneed();
            for (int i = 0; i < bin->verneednum(); ++i) {
                LOG(INFO) << "Elf_Verneed: " << SOLD_LOG_KEY(vn->vn_version) << SOLD_LOG_KEY(vn->vn_cnt)
                          << SOLD_LOG_KEY(bin->Str(vn->vn_file)) << SOLD_LOG_KEY(vn->vn_aux) << SOLD_LOG_KEY(vn->vn_next);
                vn->vn_file = strtab_builder.Add(bin->Str(vn->vn_file));
                Elf_Vernaux* vna = (Elf_Vernaux*)((char*)vn + vn->vn_aux);
                for (int j = 0; j < vn->vn_cnt; ++j) {
                    LOG(INFO) << "Elf_Vernaux: " << SOLD_LOG_KEY(vna->vna_hash) << SOLD_LOG_KEY(vna->vna_flags)
                              << SOLD_LOG_KEY(vna->vna_other) << SOLD_LOG_KEY(bin->strtab() + vna->vna_name) << SOLD_LOG_KEY(vna->vna_next);

                    if (vna->vna_other == bin->versym()[index]) {
                        LOG(INFO) << "Find Elf_Vernaux corresponds to " << bin->versym()[index] << SOLD_LOG_KEY(bin->strtab() + vn->vn_file)
                                  << SOLD_LOG_KEY(bin->Str(vna->vna_name));
                    }
                    vna->vna_name = strtab_builder.Add(bin->Str(vna->vna_name));

                    vna = (Elf_Vernaux*)((char*)vna + vna->vna_next);
                }
                vn = (Elf_Verneed*)((char*)vn + vn->vn_next);
            }
        }
    }

    // Calculate vaddrs
    strtab_builder.Freeze();
    Elf_Addr strtab_vaddr = 0;
    for (const Elf_Phdr* pp : bin->phdrs()) {
        Elf_Phdr p = *pp;
        strtab_vaddr = std::max(strtab_vaddr, AlignNext(p.p_vaddr + p.p_memsz));
    }
    Elf_Addr gnu_hash_vaddr = strtab_vaddr + strtab_builder.size();

    Elf_GnuHash gnu_hash;
    gnu_hash.nbuckets = 1;
    gnu_hash.symndx = 1;
    gnu_hash.maskwords = 1;
    gnu_hash.shift2 = 1;

    // Rewrite addresses of Phdrs
    for (const Elf_Phdr* pp : bin->phdrs()) {
        Elf_Phdr p = *pp;
        LOG(INFO) << SOLD_LOG_BITS(p.p_filesz) << SOLD_LOG_BITS(p.p_type) << SOLD_LOG_BITS(p.p_offset);

        if (p.p_type == PT_DYNAMIC) {
            CHECK_EQ(p.p_filesz % sizeof(Elf_Dyn), 0);
            LOG(INFO) << SOLD_LOG_BITS(p.p_offset);

            auto get_new_vaddr = [](Elf_Addr vaddr) {
                // TODO(akawashiro): Fix here
                if (vaddr < 0x1000) {
                    return vaddr + sizeof(Elf_Phdr);
                } else {
                    return vaddr;
                }
            };

            for (size_t i = 0; i < p.p_filesz / sizeof(Elf_Dyn); ++i) {
                Elf_Dyn* dyn = const_cast<Elf_Dyn*>(reinterpret_cast<const Elf_Dyn*>(bin->head() + p.p_offset + sizeof(Elf_Dyn) * i));
                LOG(INFO) << SOLD_LOG_BITS(dyn->d_tag) << SOLD_LOG_BITS(dyn);
                switch (dyn->d_tag) {
                    case DT_STRTAB:
                        dyn->d_un.d_ptr = strtab_vaddr;
                        LOG(INFO) << "Rewrite offset of " << ShowDynamicEntryType(dyn->d_tag);
                        break;
                    case DT_SYMTAB:
                        dyn->d_un.d_ptr = get_new_vaddr(dyn->d_un.d_ptr);
                        LOG(INFO) << "Rewrite offset of " << ShowDynamicEntryType(dyn->d_tag);
                        break;
                    case DT_GNU_HASH:
                        dyn->d_un.d_ptr = gnu_hash_vaddr;
                        LOG(INFO) << "Rewrite offset of " << ShowDynamicEntryType(dyn->d_tag);
                        break;
                    case DT_HASH:
                        dyn->d_un.d_ptr = get_new_vaddr(dyn->d_un.d_ptr);
                        LOG(INFO) << "Rewrite offset of " << ShowDynamicEntryType(dyn->d_tag);
                        CHECK(false) << "We do not support yet";
                        break;
                    case DT_RELA:
                        dyn->d_un.d_ptr = get_new_vaddr(dyn->d_un.d_ptr);
                        LOG(INFO) << "Rewrite offset of " << ShowDynamicEntryType(dyn->d_tag);
                        break;
                    case DT_VERSYM:
                        dyn->d_un.d_ptr = get_new_vaddr(dyn->d_un.d_ptr);
                        LOG(INFO) << "Rewrite offset of " << ShowDynamicEntryType(dyn->d_tag);
                        break;
                    case DT_VERNEED:
                        dyn->d_un.d_ptr = get_new_vaddr(dyn->d_un.d_ptr);
                        LOG(INFO) << "Rewrite offset of " << ShowDynamicEntryType(dyn->d_tag);
                        break;
                    case DT_VERDEF:
                        dyn->d_un.d_ptr = get_new_vaddr(dyn->d_un.d_ptr);
                        LOG(INFO) << "Rewrite offset of " << ShowDynamicEntryType(dyn->d_tag);
                        break;
                    default:
                        break;
                }
            }
        }

        // TODO(akawashiro): This is super dirty hack!!
        if (p.p_offset == 0) {
            p.p_filesz += sizeof(Elf_Phdr);
            p.p_memsz += sizeof(Elf_Phdr);
        }
        Write(fp, p);
    }

    Elf64_Addr strtab_fileoffset = AlignNext(bin->filesize(), 0x1000 - 1);
    {
        Elf_Phdr str_phdr;
        LOG(INFO) << SOLD_LOG_BITS(bin->filesize()) << SOLD_LOG_BITS(AlignNext(bin->filesize(), 0x1000 - 1));
        str_phdr.p_offset = strtab_fileoffset;
        str_phdr.p_flags = PF_R;
        str_phdr.p_vaddr = strtab_vaddr;
        str_phdr.p_paddr = strtab_vaddr;
        str_phdr.p_memsz = 0x1000;
        str_phdr.p_filesz = 0x1000;
        str_phdr.p_type = PT_LOAD;
        str_phdr.p_align = 0x1000;
        Write(fp, str_phdr);
    }
    Elf64_Addr gnu_hash_fileoffset = strtab_fileoffset + strtab_builder.size();
    Elf_Addr remaining_pad_fileoffset =
        gnu_hash_fileoffset + (sizeof(uint32_t) * 4 + sizeof(Elf_Addr) + sizeof(uint32_t) * (1 + sym_names.size() - gnu_hash.symndx));
    Elf_Addr eof_fileoffset = strtab_fileoffset + 0x1000;  // TODO(akawashiro): 0x1000 is just a temporal value.

    // WriteBuf(fp, bin->head() + sizeof(Elf_Ehdr) + sizeof(Elf_Phdr) * bin->phdrs().size(),
    //          bin->filesize() - (sizeof(Elf_Ehdr) + sizeof(Elf_Phdr) * bin->phdrs().size()));

    WriteBuf(fp, bin->head() + sizeof(Elf_Ehdr) + sizeof(Elf_Phdr) * bin->phdrs().size(),
             0x1000 - (sizeof(Elf_Ehdr) + sizeof(Elf_Phdr) * (bin->phdrs().size() + 1)));
    WriteBuf(fp, bin->head() + 0x1000, bin->filesize() - 0x1000);
    EmitPad(fp, strtab_fileoffset);

    // Emit strtab
    CHECK(ftell(fp) == strtab_fileoffset);
    strtab_builder.Freeze();
    WriteBuf(fp, strtab_builder.data(), strtab_builder.size());

    // Emit GnuHash
    CHECK(ftell(fp) == gnu_hash_fileoffset);
    Write(fp, gnu_hash.nbuckets);
    Write(fp, gnu_hash.symndx);
    Write(fp, gnu_hash.maskwords);
    Write(fp, gnu_hash.shift2);
    Elf_Addr bloom_filter = -1;
    Write(fp, bloom_filter);
    // If there is no symbols in gnu_hash, bucket must be 0.
    uint32_t bucket = (sym_indecies.size() > gnu_hash.symndx) ? gnu_hash.symndx : 0;
    Write(fp, bucket);

    for (size_t i = gnu_hash.symndx; i < sym_names.size(); ++i) {
        uint32_t h = CalcGnuHash(sym_names[i]) & ~1;
        if (i == sym_names.size() - 1) {
            h |= 1;
        }
        Write(fp, h);
    }

    // Emit pads
    CHECK(ftell(fp) == remaining_pad_fileoffset);
    EmitPad(fp, eof_fileoffset);
}

int main(int argc, char* argv[]) {
    google::InitGoogleLogging(argv[0]);

    static option long_options[] = {
        {"rename-mapping-file", required_argument, nullptr, 1},
        {"output", required_argument, nullptr, 'o'},
        {0, 0, 0, 0},
    };

    std::string input;
    std::string output;
    std::string rename_mapping_file;

    int opt;
    while ((opt = getopt_long(argc, argv, "l:", long_options, nullptr)) != -1) {
        switch (opt) {
            case 1:
                rename_mapping_file = optarg;
                break;
            case 'o':
                output = optarg;
                break;
            default:
                CHECK(false);
                break;
        }
    }

    CHECK(optind + 1 == argc);
    input = argv[optind];
    if (output == "") {
        output = input + ".renamed";
    }

    std::map<std::string, std::string> mapping;
    if (!rename_mapping_file.empty()) {
        mapping = ReadMappingFile(rename_mapping_file);
    }

    auto main_binary = ReadELF(input);
    Rename(std::move(main_binary), output, mapping);
}
