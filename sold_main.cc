// Copyright (C) 2021 The sold authors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "sold.h"

#include <getopt.h>

void print_help(std::ostream& os) {
    os << R"(usage: sold [option] [input]
Options:
-h, --help                      Show this help message and exit
-o, --output-file OUTPUT_FILE   Specify the ELF file to output (this option is mandatory)
-i, --input-file INPUT_FILE     Specify the ELF file to output
-e, --exclude-so EXCLUDE_FILE   Specify the ELF file to exclude (e.g. libmax.so) 
--section-headers               Emit section headers

The last argument is interpreted as SOURCE_FILE when -i option isn't given.
)" << std::endl;
}

int main(int argc, char* const argv[]) {
    google::InitGoogleLogging(argv[0]);

    static option long_options[] = {
        {"help", no_argument, nullptr, 'h'},
        {"input-file", required_argument, nullptr, 'i'},
        {"output-file", required_argument, nullptr, 'o'},
        {"exclude-so", required_argument, nullptr, 'e'},
        {"section-headers", no_argument, nullptr, 1},
        {0, 0, 0, 0},
    };

    std::string input_file;
    std::string output_file;
    std::vector<std::string> exclude_sos;
    bool emit_section_header = false;

    int opt;
    while ((opt = getopt_long(argc, argv, "hi:o:e:", long_options, nullptr)) != -1) {
        switch (opt) {
            case 1:
                emit_section_header = true;
                break;
            case 'e':
                exclude_sos.push_back(optarg);
                break;
            case 'i':
                input_file = optarg;
                break;
            case 'o':
                output_file = optarg;
                break;
            case 'h':
                print_help(std::cout);
                return 0;
            case '?':
                print_help(std::cerr);
                return 1;
        }
    }

    if (optind < argc && input_file.empty()) {
        input_file = argv[optind++];
    }

    if (output_file == "") {
        std::cerr << "You must specify the output file." << std::endl;
        return 1;
    }

    Sold sold(input_file, exclude_sos, emit_section_header);
    sold.Link(output_file);
    return 0;
}
