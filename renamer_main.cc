#include <getopt.h>

#include <fstream>
#include <iostream>
#include <map>
#include <set>

#include "elf_binary.h"
#include "strtab_builder.h"
#include "utils.h"

// TODO(akawashiro): Implement other methods
// You can choose a method to insert Elf_Phdr for strtab and gnu_hash.
//
// kPT_NOTE utilizes an existing Elf_Phdr of PT_NOTE. I recommend you use this
// method first.
enum class PhdrInsertStrategy { kPT_NOTE };

PhdrInsertStrategy PhdrInsertStrategyFromString(std::string str) {
    if (str == "PT_NOTE") {
        return PhdrInsertStrategy::kPT_NOTE;
    } else {
        LOG(FATAL) << str << " is not legitimate as PhdrInsertStrategy";
    }
}

std::map<std::string, std::string> ReadMappingFile(std::string file) {
    std::string line;
    std::set<std::string> froms, tos;
    std::map<std::string, std::string> res;
    std::ifstream infile(file, std::ifstream::in);
    CHECK(infile);

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

void Rename(std::unique_ptr<ELFBinary> bin, std::string outfile, std::map<std::string, std::string> mapping,
            const PhdrInsertStrategy phdr_insert_strategy) {
    StrtabBuilder strtab_builder(mapping);
    FILE* fp = fopen(outfile.c_str(), "wb");

    // Emit the new Ehdr
    {
        Elf_Ehdr e = *bin->ehdr();
        e.e_shstrndx = 0;
        e.e_shoff = 0;
        e.e_shnum = 0;
        Write(fp, e);
    }

    // Collect all names from symbols and rename them
    std::set<int> sym_indecies = bin->CollectSymbolsFromDynamic();

    // Check we collect all symbols
    CHECK_EQ(*sym_indecies.begin(), 0);
    CHECK_EQ(*sym_indecies.rbegin() + 1, sym_indecies.size());

    std::vector<std::string> sym_names;
    for (int i : sym_indecies) {
        Elf_Sym* s = bin->symtab() + i;
        std::string n = bin->Str(s->st_name);
        strtab_builder.Add(n);
        if (mapping.find(n) != mapping.end()) {
            sym_names.emplace_back(mapping[n]);
            LOG(INFO) << "rename from " << SOLD_LOG_KEY(n) << SOLD_LOG_KEY(mapping[n]);
        } else {
            sym_names.emplace_back(n);
        }
        s->st_name = strtab_builder.GetPos(n);
    }
    CHECK_EQ(std::set<std::string>(sym_names.begin(), sym_names.end()).size(), sym_names.size())
        << "There is a duplicated name in the renamed symbols";

    // Rewrite strings in dynamic
    for (const Elf_Phdr* pp : bin->phdrs()) {
        Elf_Phdr p = *pp;
        if (p.p_type == PT_DYNAMIC) {
            CHECK_EQ(p.p_filesz % sizeof(Elf_Dyn), 0);
            for (size_t i = 0; i < p.p_filesz / sizeof(Elf_Dyn); ++i) {
                Elf_Dyn* dyn = const_cast<Elf_Dyn*>(reinterpret_cast<const Elf_Dyn*>(bin->head() + p.p_offset + sizeof(Elf_Dyn) * i));
                switch (dyn->d_tag) {
                    case DT_NEEDED:
                        LOG(INFO) << "Rewrite " << ShowDynamicEntryType(DT_NEEDED);
                        dyn->d_un.d_val = strtab_builder.Add(bin->Str(dyn->d_un.d_val));
                        break;
                    case DT_SONAME:
                        LOG(INFO) << "Rewrite " << ShowDynamicEntryType(DT_SONAME);
                        dyn->d_un.d_val = strtab_builder.Add(bin->Str(dyn->d_un.d_val));
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
    for (const Elf_Phdr* p : bin->phdrs()) {
        strtab_vaddr = std::max(strtab_vaddr, AlignNext(p->p_vaddr + p->p_memsz));
    }
    Elf_Addr gnu_hash_vaddr = strtab_vaddr + strtab_builder.size();

    Elf_GnuHash gnu_hash;
    gnu_hash.nbuckets = 1;
    gnu_hash.symndx = 1;
    gnu_hash.maskwords = 1;
    gnu_hash.shift2 = 1;

    Elf_Addr strtab_fileoffset = AlignNext(bin->filesize());
    Elf_Addr gnu_hash_fileoffset = strtab_fileoffset + strtab_builder.size();
    Elf_Addr gnu_hash_filesize = (sizeof(uint32_t) * 4 + sizeof(Elf_Addr) + sizeof(uint32_t) * (1 + sym_names.size() - gnu_hash.symndx));
    Elf_Addr remaining_pad_fileoffset = gnu_hash_fileoffset + gnu_hash_filesize;
    Elf_Addr strs_size = AlignNext(strtab_builder.size() + gnu_hash_filesize);  // Sum of dynstrtab and hashes
    Elf_Addr eof_fileoffset = strtab_fileoffset + strs_size;

    // Construct Phdr for new strs
    Elf_Phdr str_phdr;
    str_phdr.p_offset = strtab_fileoffset;
    str_phdr.p_flags = PF_R;
    str_phdr.p_vaddr = strtab_vaddr;
    str_phdr.p_paddr = strtab_vaddr;
    str_phdr.p_memsz = strs_size;
    str_phdr.p_filesz = strs_size;
    str_phdr.p_type = PT_LOAD;
    str_phdr.p_align = 0x1000;

    // Rewrite addresses
    if (phdr_insert_strategy == PhdrInsertStrategy::kPT_NOTE) {
        Elf_Phdr* victim_note = nullptr;
        for (const Elf_Phdr* p : bin->phdrs()) {
            if (p->p_type == PT_NOTE) {
                victim_note = const_cast<Elf_Phdr*>(p);
            }
        }

        CHECK(victim_note != nullptr) << "Cannot find any PT_NOTE segment to rewrite";

        *victim_note = str_phdr;

        for (const Elf_Phdr* pp : bin->phdrs()) {
            Elf_Phdr p = *pp;
            if (p.p_type == PT_DYNAMIC) {
                CHECK_EQ(p.p_filesz % sizeof(Elf_Dyn), 0);
                for (size_t i = 0; i < p.p_filesz / sizeof(Elf_Dyn); ++i) {
                    Elf_Dyn* dyn = const_cast<Elf_Dyn*>(reinterpret_cast<const Elf_Dyn*>(bin->head() + p.p_offset + sizeof(Elf_Dyn) * i));
                    switch (dyn->d_tag) {
                        case DT_STRTAB:
                            dyn->d_un.d_ptr = strtab_vaddr;
                            break;
                        case DT_GNU_HASH:
                            dyn->d_un.d_ptr = gnu_hash_vaddr;
                            break;
                        case DT_HASH:
                            LOG(FATAL) << "We do not support" << ShowDynamicEntryType(DT_HASH);
                            break;
                        default:
                            break;
                    }
                }
            }

            Write(fp, p);
            LOG(INFO) << SOLD_LOG_KEY(p.p_type) << SOLD_LOG_BITS(p.p_offset) << SOLD_LOG_BITS(p.p_vaddr);
        }
    }

    if (phdr_insert_strategy == PhdrInsertStrategy::kPT_NOTE) {
        WriteBuf(fp, bin->head() + sizeof(Elf_Ehdr) + sizeof(Elf_Phdr) * bin->phdrs().size(),
                 bin->filesize() - (sizeof(Elf_Ehdr) + sizeof(Elf_Phdr) * bin->phdrs().size()));
    }
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

void print_help(std::ostream& os) {
    os << R"(usage: renamer [option] [input]
Options:
-h, --help                Show this help message and exit
-o, --output OUTPUT       Specify the ELF file to output
--rename-mapping-file     Space separated lines to specify mapping
--phdr-insert-strategy    Strategy to insert new Elf_Phdr. "PT_NOTE" or "padding"
)" << std::endl;
}

int main(int argc, char* argv[]) {
    google::InitGoogleLogging(argv[0]);

    static option long_options[] = {
        {"help", no_argument, nullptr, 'h'},
        {"phdr-insert-strategy", required_argument, nullptr, 2},
        {"rename-mapping-file", required_argument, nullptr, 1},
        {"output", required_argument, nullptr, 'o'},
        {0, 0, 0, 0},
    };

    std::string input;
    std::string output;
    std::string rename_mapping_file;
    std::string phdr_insert_strategy = "PT_NOTE";

    int opt;
    while ((opt = getopt_long(argc, argv, "l:", long_options, nullptr)) != -1) {
        switch (opt) {
            case 1:
                rename_mapping_file = optarg;
                break;
            case 2:
                phdr_insert_strategy = optarg;
                break;
            case 'o':
                output = optarg;
                break;
            case 'h':
                print_help(std::cout);
                return 0;
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
    Rename(std::move(main_binary), output, mapping, PhdrInsertStrategyFromString(phdr_insert_strategy));
}
