#include "ldsoconf.h"

#include <glob.h>

#include <fstream>

namespace ldsoconf {
namespace {
void read_ldsoconf_dfs(std::vector<std::string>& res, const std::string& filename) {
    std::ifstream f;
    f.open(filename);
    if (!f) {
        return;
    }
    std::string head;
    while (f >> head) {
        if (head.substr(0, 1) == "#") {
            std::string comment;
            std::getline(f, comment);
        } else if (head == "include") {
            std::string descendants;
            f >> descendants;

            glob_t globbuf;
            glob(descendants.c_str(), 0, NULL, &globbuf);
            for (int i = 0; i < globbuf.gl_pathc; i++) {
                read_ldsoconf_dfs(res, globbuf.gl_pathv[i]);
            }
        } else {
            res.push_back(head);
        }
    }
}

}  // namespace

std::vector<std::string> read_ldsoconf() {
    std::vector<std::string> res;
    read_ldsoconf_dfs(res, "/etc/ld.so.conf");

    return res;
}
}  // namespace ldsoconf
