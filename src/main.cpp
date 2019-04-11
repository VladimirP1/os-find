#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdlib>
#include <functional>
#include <iostream>
#include <queue>
#include <vector>

using handler_type = std::vector<std::function<bool(
    std::string filename, struct stat*, const std::string&)>>;

void handle_error(std::string msg, int code) {
    fprintf(stderr, "%s: %s\n", msg.c_str(), strerror(code));

    exit(EXIT_FAILURE);
}

void usage() {
    printf(
        "Usage: find [-inum inode] [-name filename] [-size [+-=]size] [-nlinks "
        "hardlinks] [-exec command]\n");
}

void DFS(int fd, std::string path, std::string filename,
         const handler_type& tests) {
    DIR* dir;
    bool print = true;
    struct dirent* chld_dirent;
    struct stat st;

    int ret = fstat(fd, &st);

    if (ret < 0) {
        fprintf(stderr, "Could not stat %s: %s\n", path.c_str(),
                strerror(errno));
        goto error_stat;
    }

    for (const auto& c : tests) {
        print &= !!c(filename, &st, path);
    }

    if (print) {
        printf("%s\n", path.c_str());
    }

    if (!S_ISDIR(st.st_mode)) {
        return;
    }

    dir = fdopendir(fd);

    while (true) {
        chld_dirent = readdir(dir);

        if (!chld_dirent) {
            break;
        }
        std::string chld_path = path + "/" + chld_dirent->d_name;

        int chld_fd = openat(fd, chld_dirent->d_name, O_RDONLY | O_NOFOLLOW);

        if (chld_fd < 0 && errno == ELOOP) {
            chld_fd = openat(fd, chld_dirent->d_name, O_PATH | O_NOFOLLOW);
        }

        if (chld_fd < 0) {
            fprintf(stderr, "Could not open file %s: %s\n", chld_path.c_str(),
                    strerror(errno));
            goto error_open;
        }

        if (strcmp(chld_dirent->d_name, ".") &&
            strcmp(chld_dirent->d_name, "..")) {
            DFS(chld_fd, chld_path, chld_dirent->d_name, tests);
        }

        close(chld_fd);
    error_open:;
    }

    closedir(dir);
error_stat:;
}

int main(int argc, char** argv) {
    int i = 0;
    handler_type tests;

    if (argc & 1 || argc < 2) {
        fprintf(stderr, "Invalid argument count\n\n");
        usage();
        return 1;
    }

    for (i = 2; i < argc - 1; i += 2) {
        if (!strcmp(argv[i], "-inum")) {
            tests.push_back(
                [ino = atoi(argv[i + 1])](
                    const std::string& filename, struct stat* st,
                    const std::string& path) { return st->st_ino == ino; });
        } else if (!strcmp(argv[i], "-name")) {
            tests.push_back([name = argv[i + 1]](const std::string& filename,
                                                 struct stat* st,
                                                 const std::string& path) {

                return !strcmp(filename.c_str(), name);
            });
        } else if (!strcmp(argv[i], "-size")) {
            tests.push_back(
                [arg = argv[i + 1],
                 argi = (argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9')
                            ? atoi(argv[i + 1])
                            : atoi(argv[i + 1] + 1)](
                    const std::string& filename, struct stat* st,
                    const std::string& path) {

                    switch (arg[0]) {
                        case '+':
                            return st->st_size > argi;
                        case '-':
                            return st->st_size < argi;
                        case '=':
                            return st->st_size == argi;
                        default:
                            return st->st_size > argi;
                    }
                });
        } else if (!strcmp(argv[i], "-nlinks")) {
            tests.push_back([nlinks = atoll(argv[i + 1])](
                                const std::string& filename, struct stat* st,
                                const std::string& path) {
                return st->st_nlink == nlinks;
            });
        } else if (!strcmp(argv[i], "-exec")) {
            tests.push_back([cmd = std::string(argv[i + 1])](
                                const std::string& filename, struct stat* st,
                                const std::string& path) {
                std::system((cmd + " " + path).c_str());
                return true;
            });
        } else {
            fprintf(stderr, "Invalid argument: %s\n\n", argv[i]);
            usage();
            exit(EXIT_FAILURE);
        }
    }

    int fd = open(argv[1], O_RDONLY);

    if (fd < 0) {
        handle_error("Could not open direcotry", errno);
    }

    DFS(fd, argv[1], argv[1], tests);
}
