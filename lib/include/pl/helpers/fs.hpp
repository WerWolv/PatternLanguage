#pragma once

#include <optional>
#include <string>
#include <vector>
#include <filesystem>
#include <functional>

namespace std::fs {
    using namespace std::filesystem;
}

namespace pl::hlp::fs {

    [[maybe_unused]]
    static inline bool exists(const std::fs::path &path) {
        std::error_code error;
        return std::filesystem::exists(path, error) && !error;
    }

    [[maybe_unused]]
    static inline bool createDirectories(const std::fs::path &path) {
        std::error_code error;
        return std::filesystem::create_directories(path, error) && !error;
    }

    [[maybe_unused]]
    static inline bool isRegularFile(const std::fs::path &path) {
        std::error_code error;
        return std::filesystem::is_regular_file(path, error) && !error;
    }

    [[maybe_unused]]
    static inline bool copyFile(const std::fs::path &from, const std::fs::path &to, std::fs::copy_options = std::fs::copy_options::none) {
        std::error_code error;
        return std::filesystem::copy_file(from, to, error) && !error;
    }

    [[maybe_unused]]
    static inline bool isDirectory(const std::fs::path &path) {
        std::error_code error;
        return std::filesystem::is_directory(path, error) && !error;
    }

    [[maybe_unused]]
    static inline bool remove(const std::fs::path &path) {
        std::error_code error;
        return std::filesystem::remove(path, error) && !error;
    }

    [[maybe_unused]]
    static inline uintmax_t getFileSize(const std::fs::path &path) {
        std::error_code error;
        auto size = std::filesystem::file_size(path, error);

        if (error) return 0;
        else return size;
    }

}