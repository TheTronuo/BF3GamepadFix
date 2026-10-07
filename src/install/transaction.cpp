#include <windows.h>
#include "bf3/install/transaction.hpp"
#include "bf3/hash.hpp"
#include <algorithm>
#include <cstring>
#include <exception>
#include <fstream>
#include <memory>

namespace bf3::install {
namespace fs = std::filesystem;
namespace {
struct CloseHandleDeleter {
    void operator()(void* handle) const noexcept { CloseHandle(handle); }
};
using Handle = std::unique_ptr<void, CloseHandleDeleter>;

Handle open_archive(const fs::path& path) {
    const auto handle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
                                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Cannot lock archive; close the game: " + path.string());
    return Handle(handle);
}

void seek(HANDLE file, std::uint64_t offset) {
    LARGE_INTEGER position{};
    position.QuadPart = static_cast<LONGLONG>(offset);
    if (!SetFilePointerEx(file, position, nullptr, FILE_BEGIN))
        throw std::runtime_error("Archive seek failed");
}

void verify_range(HANDLE file, const RangeChange& range, std::uint64_t size) {
    if (range.before.size() != range.after.size() || range.offset > size ||
        range.before.size() > size - range.offset || range.before.size() > MAXDWORD)
        throw std::runtime_error("Invalid fixed-size patch range");
    seek(file, range.offset);
    Bytes current(range.before.size());
    DWORD read = 0;
    if (!ReadFile(file, current.data(), static_cast<DWORD>(current.size()), &read, nullptr) ||
        read != current.size() || current != range.before)
        throw std::runtime_error("Archive range changed before installation");
}

void write_range(HANDLE file, const RangeChange& range, bool restore) {
    seek(file, range.offset);
    const auto& bytes = restore ? range.before : range.after;
    DWORD written = 0;
    if (!WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) ||
        written != bytes.size())
        throw std::runtime_error("Archive write failed");
}

Bytes read_file(const fs::path& path) {
    const auto size = fs::file_size(path);
    if (size > 16 * 1024 * 1024)
        throw std::runtime_error("Replacement file exceeds the supported limit");
    Bytes bytes(static_cast<std::size_t>(size));
    std::ifstream input(path, std::ios::binary);
    if (!input || !input.read(reinterpret_cast<char*>(bytes.data()),
                              static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("Cannot read replacement file");
    return bytes;
}

void replace_file(const fs::path& path, const Bytes& bytes) {
    fs::create_directories(path.parent_path());
    auto temporary = path;
    temporary += L".bf3gamepadfix-tmp";
    if (fs::exists(temporary))
        throw std::runtime_error("A previous temporary file exists: " + temporary.string());
    try {
        std::ofstream output(temporary, std::ios::binary);
        if (!output || !output.write(reinterpret_cast<const char*>(bytes.data()),
                                     static_cast<std::streamsize>(bytes.size())))
            throw std::runtime_error("Cannot write replacement file");
        output.close();
        if (!output)
            throw std::runtime_error("Cannot flush replacement file");
        if (!MoveFileExW(temporary.c_str(), path.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot commit replacement file: " + path.string() +
                                     " (Windows error " + std::to_string(GetLastError()) + ")");
    } catch (...) {
        std::error_code ignored;
        fs::remove(temporary, ignored);
        throw;
    }
}
} // namespace

fs::path contained_path(const fs::path& root, const fs::path& relative) {
    if (relative.empty() || relative.is_absolute())
        throw std::runtime_error("Expected a relative path inside the game directory");
    const auto canonical_root = fs::weakly_canonical(root);
    const auto target = fs::weakly_canonical(canonical_root / relative);
    auto root_part = canonical_root.begin();
    auto target_part = target.begin();
    for (; root_part != canonical_root.end(); ++root_part, ++target_part) {
        if (target_part == target.end() || _wcsicmp(root_part->c_str(), target_part->c_str()) != 0)
            throw std::runtime_error("Path escapes the game directory");
    }
    if (target_part == target.end())
        throw std::runtime_error("A file path must not be the game directory itself");
    return target;
}

void apply_transaction(const std::vector<ArchiveChange>& archives,
                       const std::vector<FileChange>& files,
                       const std::function<void()>& after_range_write) {
    std::vector<Handle> handles;
    for (const auto& archive : archives) {
        handles.push_back(open_archive(archive.path));
        if (fs::file_size(archive.path) != archive.size ||
            file_sha256(archive.path) != archive.before_sha256)
            throw std::runtime_error("Unsupported or changed archive: " + archive.path.string());
        for (std::size_t i = 0; i < archive.ranges.size(); ++i) {
            const auto& range = archive.ranges[i];
            verify_range(handles.back().get(), range, archive.size);
            for (std::size_t j = 0; j < i; ++j) {
                const auto& other = archive.ranges[j];
                if (range.offset < other.offset + other.before.size() &&
                    other.offset < range.offset + range.before.size())
                    throw std::runtime_error("Overlapping patch ranges");
            }
        }
    }
    for (const auto& file : files) {
        if (fs::exists(file.path) != file.existed ||
            (file.existed && read_file(file.path) != file.before))
            throw std::runtime_error("Replacement file changed before installation");
    }
    std::size_t files_started = 0;
    try {
        for (std::size_t i = 0; i < archives.size(); ++i) {
            for (const auto& range : archives[i].ranges) {
                write_range(handles[i].get(), range, false);
                if (after_range_write)
                    after_range_write();
            }
            if (!FlushFileBuffers(handles[i].get()) ||
                file_sha256(archives[i].path) != archives[i].after_sha256)
                throw std::runtime_error("Patched archive verification failed");
        }
        for (const auto& file : files) {
            replace_file(file.path, file.after);
            ++files_started;
        }
    } catch (...) {
        const auto failure = std::current_exception();
        try {
            for (std::size_t i = 0; i < archives.size(); ++i) {
                for (const auto& range : archives[i].ranges)
                    write_range(handles[i].get(), range, true);
                if (!FlushFileBuffers(handles[i].get()) ||
                    file_sha256(archives[i].path) != archives[i].before_sha256)
                    throw std::runtime_error("Rollback archive verification failed");
            }
            for (std::size_t i = files_started; i > 0; --i) {
                const auto& file = files[i - 1];
                if (file.existed)
                    replace_file(file.path, file.before);
                else
                    fs::remove(file.path);
            }
        } catch (const std::exception& restore_error) {
            throw std::runtime_error(std::string("Recovery requires attention: ") +
                                     restore_error.what());
        }
        std::rethrow_exception(failure);
    }
}
} // namespace bf3::install
