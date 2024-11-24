#include "rec_dir_it.hpp"
#include <stdexcept>

namespace stdlike {

directory::directory(const std::filesystem::path& p, const struct stat& st, bool is_symlink)
    : _path(p), _is_symlink(is_symlink) {
    _is_directory = S_ISDIR(st.st_mode);
    _is_regular_file = S_ISREG(st.st_mode);
    _is_block_file = S_ISBLK(st.st_mode);
    _is_character_file = S_ISCHR(st.st_mode);
    _is_socket = S_ISSOCK(st.st_mode);
    _is_fifo = S_ISFIFO(st.st_mode);
    _file_size = _is_regular_file ? st.st_size : 0;
    _hard_link_count = st.st_nlink;
    _last_write_time = st.st_mtime;
}

std::filesystem::path directory::path() const {
    return _path;
}

bool directory::is_directory() const {
    return _is_directory;
}

bool directory::is_regular_file() const {
    return _is_regular_file;
}

bool directory::is_symlink() const {
    return _is_symlink;
}

bool directory::is_block_file() const {
    return _is_block_file;
}

bool directory::is_character_file() const {
    return _is_character_file;
}

bool directory::is_socket() const {
    return _is_socket;
}

bool directory::is_fifo() const {
    return _is_fifo;
}

int directory::file_size() const {
    return _file_size;
}

int directory::hard_link_count() const {
    return _hard_link_count;
}

std::time_t directory::last_write_time() const {
    return _last_write_time;
}

bool HasOption(directory_options options, directory_options flag) {
    return (options & flag) == flag;
}

struct recursive_directory_iterator::iterator_impl {
    std::stack<std::pair<DIR*, std::filesystem::path>> dir_stack;
    directory current_entry;
    directory_options options;
    int current_depth = 0;
    bool at_end = false;

    iterator_impl() : options(directory_options::none), at_end(true) {
    }

    iterator_impl(const std::filesystem::path& path, directory_options opts) : options(opts) {
        struct stat st;
        if (lstat(path.c_str(), &st) == -1) {
            if (!HasOption(options, directory_options::skip_permission_denied)) {
                throw std::runtime_error("Нет доступа к директории: " + path.string());
            } else {
                at_end = true;
                return;
            }
        }

        if (!S_ISDIR(st.st_mode)) {
            throw std::runtime_error("Путь не является директорией: " + path.string());
        }

        DIR* dir = opendir(path.c_str());
        if (dir != nullptr) {
            dir_stack.emplace(dir, path);
            Advance();
        } else {
            if (!HasOption(options, directory_options::skip_permission_denied)) {
                throw std::runtime_error("Не удалось открыть директорию: " + path.string());
            } else {
                at_end = true;
            }
        }
    }

    ~iterator_impl() {
        while (!dir_stack.empty()) {
            closedir(dir_stack.top().first);
            dir_stack.pop();
        }
    }

    void Advance() {
        while (!dir_stack.empty()) {
            DIR* current_dir = dir_stack.top().first;
            std::filesystem::path current_path = dir_stack.top().second;

            errno = 0;
            struct dirent* entry = readdir(current_dir);
            if (entry == nullptr) {
                if (errno != 0) {
                    if (!HasOption(options, directory_options::skip_permission_denied)) {
                        throw std::runtime_error("Ошибка чтения директории: " +
                                                 current_path.string());
                    }
                }
                closedir(current_dir);
                dir_stack.pop();
                current_depth--;
                continue;
            }

            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            std::filesystem::path full_path = current_path / entry->d_name;

            struct stat st;
            if (lstat(full_path.c_str(), &st) == -1) {
                if (!HasOption(options, directory_options::skip_permission_denied)) {
                    throw std::runtime_error("Ошибка lstat для файла: " + full_path.string());
                }
                continue;
            }

            bool is_symlink = S_ISLNK(st.st_mode);
            bool should_follow_symlink =
                is_symlink && HasOption(options, directory_options::follow_directory_symlink);

            if (should_follow_symlink) {
                struct stat target_st;
                if (stat(full_path.c_str(), &target_st) == -1) {
                    if (!HasOption(options, directory_options::skip_permission_denied)) {
                        throw std::runtime_error("Ошибка stat для символической ссылки: " +
                                                 full_path.string());
                    }
                    continue;
                }

                if (S_ISDIR(target_st.st_mode)) {
                    DIR* subdir = opendir(full_path.c_str());
                    if (subdir != nullptr) {
                        dir_stack.emplace(subdir, full_path);
                        current_depth++;
                    } else {
                        if (!HasOption(options, directory_options::skip_permission_denied)) {
                            throw std::runtime_error("Не удалось открыть директорию: " +
                                                     full_path.string());
                        }
                    }

                    directory dir_entry(full_path, target_st, is_symlink);
                    current_entry = dir_entry;

                    return;
                }
            }
            directory dir_entry(full_path, st, is_symlink);
            current_entry = dir_entry;

            if (S_ISDIR(st.st_mode)) {
                DIR* subdir = opendir(full_path.c_str());
                if (subdir != nullptr) {
                    dir_stack.emplace(subdir, full_path);
                    current_depth++;
                } else {
                    if (!HasOption(options, directory_options::skip_permission_denied)) {
                        throw std::runtime_error("Не удалось открыть директорию: " +
                                                 full_path.string());
                    }
                }
            }
            return;
        }
        at_end = true;
    }
};

recursive_directory_iterator::recursive_directory_iterator()
    : _impl(std::make_shared<iterator_impl>()) {
}

recursive_directory_iterator::recursive_directory_iterator(const char* path,
                                                           directory_options options)
    : _impl(std::make_shared<iterator_impl>(std::filesystem::path(path), options)) {
}

recursive_directory_iterator::recursive_directory_iterator(const std::filesystem::path& path,
                                                           directory_options options)
    : _impl(std::make_shared<iterator_impl>(path, options)) {
}

recursive_directory_iterator::recursive_directory_iterator(
    const recursive_directory_iterator& other)
    : _impl(other._impl) {
}

recursive_directory_iterator& recursive_directory_iterator::operator=(
    const recursive_directory_iterator& other) {
    _impl = other._impl;
    return *this;
}

recursive_directory_iterator::recursive_directory_iterator(recursive_directory_iterator&& other)
    : _impl(std::move(other._impl)) {
}

recursive_directory_iterator& recursive_directory_iterator::operator=(
    recursive_directory_iterator&& other) {
    _impl = std::move(other._impl);
    return *this;
}

directory recursive_directory_iterator::operator*() const {
    return _impl->current_entry;
}

directory* recursive_directory_iterator::operator->() const {
    return static_cast<directory*>(&_impl->current_entry);
}

recursive_directory_iterator& recursive_directory_iterator::operator++() {
    if (_impl != nullptr && !_impl->at_end) {
        _impl->Advance();

        if (_impl->at_end) {
            _impl.reset();
        }
    }
    return *this;
}

bool recursive_directory_iterator::operator==(const recursive_directory_iterator& other) const {
    if (is_end() && other.is_end()) {
        return true;
    }

    if (is_end() || other.is_end()) {
        return false;
    }

    return (_impl->current_entry.path() == other._impl->current_entry.path());
}

bool recursive_directory_iterator::operator!=(const recursive_directory_iterator& other) const {
    return !(*this == other);
}

int recursive_directory_iterator::depth() const {
    if (_impl != nullptr && !_impl->at_end) {
        return _impl->current_depth;
    }
    return -1;
}

void recursive_directory_iterator::pop() {
    if (_impl != nullptr && !_impl->dir_stack.empty()) {
        closedir(_impl->dir_stack.top().first);
        _impl->dir_stack.pop();
        _impl->current_depth--;
        _impl->Advance();
        if (_impl->at_end) {
            _impl.reset();
        }
    }
}

bool recursive_directory_iterator::is_end() const {
    return _impl == nullptr || _impl->at_end;
}

recursive_directory_iterator recursive_directory_iterator::begin() {
    return *this;
}

recursive_directory_iterator recursive_directory_iterator::end() {
    return recursive_directory_iterator();
}

}  // namespace stdlike