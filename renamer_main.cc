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
        if (froms.insert(from).second) {
            CHECK(false) << from << " is duplicated.";
        }
        if (tos.insert(to).second) {
            CHECK(false) << to << " is duplicated.";
        }
        res[from] = to;
    }
    return res;
}

void Rename(std::unique_ptr<ELFBinary> input_binary, std::string outfile, std::map<std::string, std::string> mapping) {
    StrtabBuilder strtab_builder;

    FILE* fp = fopen(outfile.c_str(), "wb");

    // Emit the new Ehdr
    {
        Elf_Ehdr e = *input_binary->ehdr();
        e.e_shstrndx = 0;
        e.e_shoff = 0;
        e.e_shnum = 0;
        e.e_phnum++;
        Write(fp, e);
    }

    // Collect all names from symbols and rename them
    std::set<int> sym_indecies = input_binary->CollectSymbolsFromDynamic();
    std::set<std::string> names;
    for (int i : sym_indecies) {
        Elf_Sym* s = input_binary->symtab() + i;
        std::string n = input_binary->Str(s->st_name);
        strtab_builder.Add(n);
        s->st_name = strtab_builder.GetPos(n);
        LOG(INFO) << SOLD_LOG_KEY(n);
    }

    // Rewrite strings in dynamic
    for (const Elf_Phdr* pp : input_binary->phdrs()) {
        Elf_Phdr p = *pp;
        LOG(INFO) << SOLD_LOG_BITS(p.p_filesz) << SOLD_LOG_BITS(p.p_type) << SOLD_LOG_BITS(p.p_offset);

        if (p.p_type == PT_DYNAMIC) {
            CHECK_EQ(p.p_filesz % sizeof(Elf_Dyn), 0);
            LOG(INFO) << SOLD_LOG_BITS(p.p_offset);

            for (size_t i = 0; i < p.p_filesz / sizeof(Elf_Dyn); ++i) {
                Elf_Dyn* dyn =
                    const_cast<Elf_Dyn*>(reinterpret_cast<const Elf_Dyn*>(input_binary->head() + p.p_offset + sizeof(Elf_Dyn) * i));
                LOG(INFO) << SOLD_LOG_BITS(dyn->d_tag) << SOLD_LOG_BITS(dyn);
                switch (dyn->d_tag) {
                    case DT_NEEDED:
                        dyn->d_un.d_val = strtab_builder.Add(input_binary->Str(dyn->d_un.d_val));
                        LOG(INFO) << "Rewrite name of " << ShowDynamicEntryType(dyn->d_tag);
                        break;
                    case DT_SONAME:
                        dyn->d_un.d_val = strtab_builder.Add(input_binary->Str(dyn->d_un.d_val));
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
        if (input_binary->verneed()) {
            Elf_Verneed* vn = input_binary->verneed();
            for (int i = 0; i < input_binary->verneednum(); ++i) {
                LOG(INFO) << "Elf_Verneed: " << SOLD_LOG_KEY(vn->vn_version) << SOLD_LOG_KEY(vn->vn_cnt)
                          << SOLD_LOG_KEY(input_binary->strtab() + vn->vn_file) << SOLD_LOG_KEY(vn->vn_aux) << SOLD_LOG_KEY(vn->vn_next);
                vn->vn_file = strtab_builder.Add(input_binary->Str(vn->vn_file));
                Elf_Vernaux* vna = (Elf_Vernaux*)((char*)vn + vn->vn_aux);
                for (int j = 0; j < vn->vn_cnt; ++j) {
                    LOG(INFO) << "Elf_Vernaux: " << SOLD_LOG_KEY(vna->vna_hash) << SOLD_LOG_KEY(vna->vna_flags)
                              << SOLD_LOG_KEY(vna->vna_other) << SOLD_LOG_KEY(input_binary->strtab() + vna->vna_name)
                              << SOLD_LOG_KEY(vna->vna_next);

                    if (vna->vna_other == input_binary->versym()[index]) {
                        LOG(INFO) << "Find Elf_Vernaux corresponds to " << input_binary->versym()[index]
                                  << SOLD_LOG_KEY(input_binary->strtab() + vn->vn_file)
                                  << SOLD_LOG_KEY(input_binary->strtab() + vna->vna_name);
                    }
                    vna->vna_name = strtab_builder.Add(input_binary->Str(vna->vna_name));

                    vna = (Elf_Vernaux*)((char*)vna + vna->vna_next);
                }
                vn = (Elf_Verneed*)((char*)vn + vn->vn_next);
            }
        }
    }

    // Calculate vaddr for the new strtab
    Elf64_Addr str_vaddr = 0;
    for (const Elf_Phdr* pp : input_binary->phdrs()) {
        Elf_Phdr p = *pp;
        str_vaddr = std::max(str_vaddr, AlignNext(p.p_vaddr + p.p_memsz));
    }

    // Rewrite addresses of Phdrs
    for (const Elf_Phdr* pp : input_binary->phdrs()) {
        Elf_Phdr p = *pp;
        LOG(INFO) << SOLD_LOG_BITS(p.p_filesz) << SOLD_LOG_BITS(p.p_type) << SOLD_LOG_BITS(p.p_offset);

        if (p.p_type == PT_DYNAMIC) {
            CHECK_EQ(p.p_filesz % sizeof(Elf_Dyn), 0);
            LOG(INFO) << SOLD_LOG_BITS(p.p_offset);

            auto update = [](Elf_Dyn* dyn) {
                if (dyn->d_un.d_ptr < 0x1000) {
                    dyn->d_un.d_ptr += sizeof(Elf_Phdr);
                }
            };

            for (size_t i = 0; i < p.p_filesz / sizeof(Elf_Dyn); ++i) {
                Elf_Dyn* dyn =
                    const_cast<Elf_Dyn*>(reinterpret_cast<const Elf_Dyn*>(input_binary->head() + p.p_offset + sizeof(Elf_Dyn) * i));
                LOG(INFO) << SOLD_LOG_BITS(dyn->d_tag) << SOLD_LOG_BITS(dyn);
                switch (dyn->d_tag) {
                    case DT_STRTAB:
                        // TODO(akawashiro): 0x5000 is a just temporal offset.
                        dyn->d_un.d_ptr = 0x5000;
                        LOG(INFO) << "Rewrite offset of " << ShowDynamicEntryType(dyn->d_tag);
                        break;
                    case DT_SYMTAB:
                        update(dyn);
                        LOG(INFO) << "Rewrite offset of " << ShowDynamicEntryType(dyn->d_tag);
                        break;
                    case DT_GNU_HASH:
                        update(dyn);
                        LOG(INFO) << "Rewrite offset of " << ShowDynamicEntryType(dyn->d_tag);
                        break;
                    case DT_HASH:
                        update(dyn);
                        LOG(INFO) << "Rewrite offset of " << ShowDynamicEntryType(dyn->d_tag);
                        break;
                    case DT_RELA:
                        update(dyn);
                        LOG(INFO) << "Rewrite offset of " << ShowDynamicEntryType(dyn->d_tag);
                        break;
                    case DT_VERSYM:
                        update(dyn);
                        LOG(INFO) << "Rewrite offset of " << ShowDynamicEntryType(dyn->d_tag);
                        break;
                    case DT_VERNEED:
                        update(dyn);
                        LOG(INFO) << "Rewrite offset of " << ShowDynamicEntryType(dyn->d_tag);
                        break;
                    case DT_VERDEF:
                        update(dyn);
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

    Elf64_Addr str_fileoffset = AlignNext(input_binary->filesize(), 0x1000 - 1);
    {
        Elf_Phdr str_phdr;
        LOG(INFO) << SOLD_LOG_BITS(input_binary->filesize()) << SOLD_LOG_BITS(AlignNext(input_binary->filesize(), 0x1000 - 1));
        str_phdr.p_offset = str_fileoffset;
        str_phdr.p_flags = PF_R;
        str_phdr.p_vaddr = str_vaddr;
        str_phdr.p_paddr = str_vaddr;
        str_phdr.p_memsz = 0x1000;
        str_phdr.p_filesz = 0x1000;
        str_phdr.p_type = PT_LOAD;
        str_phdr.p_align = 0x1000;
        Write(fp, str_phdr);
    }

    // WriteBuf(fp, input_binary->head() + sizeof(Elf_Ehdr) + sizeof(Elf_Phdr) * input_binary->phdrs().size(),
    //          input_binary->filesize() - (sizeof(Elf_Ehdr) + sizeof(Elf_Phdr) * input_binary->phdrs().size()));

    WriteBuf(fp, input_binary->head() + sizeof(Elf_Ehdr) + sizeof(Elf_Phdr) * input_binary->phdrs().size(),
             0x1000 - (sizeof(Elf_Ehdr) + sizeof(Elf_Phdr) * (input_binary->phdrs().size() + 1)));
    WriteBuf(fp, input_binary->head() + 0x1000, input_binary->filesize() - 0x1000);
    EmitPad(fp, str_fileoffset);

    strtab_builder.Freeze();
    WriteBuf(fp, strtab_builder.data(), strtab_builder.size());
    EmitPad(fp, str_fileoffset + 0x1000 - strtab_builder.size());
}

int main(int argc, char* argv[]) {
    google::InitGoogleLogging(argv[0]);

    static option long_options[] = {
        {"rewrite-mapping-file", required_argument, nullptr, 1},
        {"output", required_argument, nullptr, 'o'},
        {0, 0, 0, 0},
    };

    std::string input;
    std::string output;
    std::string rewrite_mapping_file;

    int opt;
    while ((opt = getopt_long(argc, argv, "l:", long_options, nullptr)) != -1) {
        switch (opt) {
            case 1:
                rewrite_mapping_file = optarg;
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

    auto main_binary = ReadELF(input);
    Rename(std::move(main_binary), output, {});
}
