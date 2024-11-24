#pragma once

#include <filesystem>
#include <string>
#include <stack>
#include <memory>
#include <sys/stat.h>
#include <dirent.h>
#include <cstring>
#include <cstdint>
#include <ctime>
#include <unordered_set>

namespace stdlike {

enum class directory_options {
    none = 0,
    follow_directory_symlink = 1 << 0,
    skip_permission_denied = 1 << 1,
};

inline directory_options operator|(directory_options a, directory_options b) {
    return static_cast<directory_options>(static_cast<int>(a) | static_cast<int>(b));
}

inline directory_options operator&(directory_options a, directory_options b) {
    return static_cast<directory_options>(static_cast<int>(a) & static_cast<int>(b));
}

inline directory_options operator^(directory_options a, directory_options b) {
    return static_cast<directory_options>(static_cast<int>(a) ^ static_cast<int>(b));
}

inline directory_options& operator|=(directory_options& a, directory_options b) {
    a = a | b;
    return a;
}

inline directory_options& operator&=(directory_options& a, directory_options b) {
    a = a & b;
    return a;
}

inline directory_options& operator^=(directory_options& a, directory_options b) {
    a = a ^ b;
    return a;
}

class directory {
public:
    directory() = default;
    directory(const std::filesystem::path& p, const struct stat& st, bool is_symlink = false);

    std::filesystem::path path() const;
    bool is_directory() const;
    bool is_symlink() const;
    bool is_regular_file() const;
    bool is_block_file() const;
    bool is_character_file() const;

    bool is_socket() const;
    bool is_fifo() const;

    int file_size() const;
    int hard_link_count() const;

    std::time_t last_write_time() const;

private:
    std::filesystem::path _path;
    bool _is_symlink = false;
    bool _is_directory = false;
    bool _is_regular_file = false;
    bool _is_block_file = false;
    bool _is_character_file = false;
    bool _is_socket = false;
    bool _is_fifo = false;
    int _file_size = 0;
    int _hard_link_count = 0;
    std::time_t _last_write_time = 0;
};

class recursive_directory_iterator {
public:
    recursive_directory_iterator();
    recursive_directory_iterator(const char* path, directory_options options = directory_options::none);
    recursive_directory_iterator(const std::filesystem::path& path, directory_options options = directory_options::none);

    recursive_directory_iterator(const recursive_directory_iterator& other);
    recursive_directory_iterator& operator=(const recursive_directory_iterator& other);

    recursive_directory_iterator(recursive_directory_iterator&& other);
    recursive_directory_iterator& operator=(recursive_directory_iterator&& other);

    directory operator*() const;
    directory* operator->() const;

    recursive_directory_iterator& operator++();

    bool operator==(const recursive_directory_iterator& other) const;
    bool operator!=(const recursive_directory_iterator& other) const;

    int depth() const;
    void pop();

    bool is_end() const;

    recursive_directory_iterator begin();
    recursive_directory_iterator end();

private:
    struct iterator_impl;
    std::shared_ptr<iterator_impl> _impl;
};

}