#include "engine/assets.hpp"
#include "test.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdarg>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>

extern "C" {
int __real_mkstemp(char*);
ssize_t __real_write(int, const void*, std::size_t);
int __real_fsync(int);
int __real_close(int);
int __real_rename(const char*, const char*);
int __real_open(const char*, int, ...);
int __real_unlink(const char*);
}

namespace {
using namespace engine;
using namespace testing;
enum class Operation { create, write, file_sync, file_close, rename, directory_open,
                       directory_sync, directory_close, unlink, file_observer, rename_observer };
struct Faults {
    std::string directory;
    std::array<char, 4096> temporary{};
    std::array<Operation, 64> events{};
    std::size_t event_count{}, writes{};
    Operation fail{Operation::create};
    int error{}, file{-1}, directory_fd{-1}, sentinel{-1};
    bool fragment{}, zero_write{}, cleanup_close_error{}, cleanup_unlink_error{}, reuse_closed{};
    bool file_open{}, directory_open{}, overflow{};
    unsigned sentinel_closes{};
    void record(Operation op) {
        if (event_count == events.size())
            overflow = true;
        else
            events[event_count++] = op;
    }
    bool fails(Operation op) const { return error != 0 && fail == op; }
};
Faults* active{}; // This executable is single-threaded; injection is scoped to one write.
int failure(int code) {
    errno = code;
    return -1;
}
struct Injection {
    Faults& faults;
    explicit Injection(Faults& value) : faults(value) {
        CHECK(!active);
        active = &faults;
    }
    ~Injection() {
        active = nullptr;
        if (faults.sentinel >= 0)
            __real_close(faults.sentinel);
        // Release descriptors on a failing assertion without concealing the failure.
        if (faults.file_open)
            __real_close(faults.file);
        if (faults.directory_open)
            __real_close(faults.directory_fd);
    }
};
struct Temporary {
    std::filesystem::path path;
    Temporary() {
        std::string pattern =
            (std::filesystem::temp_directory_path() / "engine-atomic-fault-XXXXXX").string();
        CHECK(mkdtemp(pattern.data()));
        path = pattern;
    }
    ~Temporary() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
    std::size_t entries() const {
        return static_cast<std::size_t>(std::distance(std::filesystem::directory_iterator(path),
                                                     std::filesystem::directory_iterator{}));
    }
};
constexpr std::string_view old_text = "previous committed record";
constexpr std::string_view new_text = "replacement with a different payload";
std::span<const std::uint8_t> bytes(std::string_view text) {
    return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
}
void observe(AtomicWriteStage stage) {
    CHECK(active);
    active->record(stage == AtomicWriteStage::file_synced ? Operation::file_observer
                                                        : Operation::rename_observer);
}
template <class F> void expect_error(F function, int code, std::string_view operation) {
    bool caught = false;
    try {
        function();
    } catch (const std::system_error& e) {
        CHECK(e.code() == std::error_code(code, std::generic_category()));
        CHECK(std::string_view(e.what()).find(operation) != std::string_view::npos);
        caught = true;
    }
    CHECK(caught);
}
void closed(const Faults& f) {
    CHECK(!f.file_open && !f.directory_open && !f.overflow);
    CHECK(f.sentinel_closes == 0);
}
} // namespace

extern "C" int __wrap_mkstemp(char* path) {
    if (!active)
        return __real_mkstemp(path);
    auto& f = *active;
    f.record(Operation::create);
    if (f.fails(Operation::create))
        return failure(f.error);
    const int fd = __real_mkstemp(path);
    if (fd >= 0) {
        f.file = fd;
        f.file_open = true;
        std::strncpy(f.temporary.data(), path, f.temporary.size() - 1);
    }
    return fd;
}
extern "C" ssize_t __wrap_write(int fd, const void* data, std::size_t size) {
    if (!active || !active->file_open || fd != active->file)
        return __real_write(fd, data, size);
    auto& f = *active;
    f.record(Operation::write);
    ++f.writes;
    if (f.zero_write) {
        errno = f.error; // A zero result need not set errno; test both zero and stale errno.
        return 0;
    }
    if (f.fails(Operation::write)) {
        if (f.writes == 1)
            return __real_write(fd, data, std::min(size, std::size_t{5}));
        return failure(f.error);
    }
    if (f.fragment && (f.writes == 1 || f.writes == 3))
        return failure(EINTR);
    return __real_write(fd, data, f.fragment ? std::min(size, std::size_t{4}) : size);
}
extern "C" int __wrap_fsync(int fd) {
    if (!active)
        return __real_fsync(fd);
    auto& f = *active;
    const auto op = f.directory_open && fd == f.directory_fd ? Operation::directory_sync
                                                            : Operation::file_sync;
    if (!(f.file_open && fd == f.file) && !(f.directory_open && fd == f.directory_fd))
        return __real_fsync(fd);
    f.record(op);
    return f.fails(op) ? failure(f.error) : __real_fsync(fd);
}
extern "C" int __wrap_close(int fd) {
    if (!active)
        return __real_close(fd);
    auto& f = *active;
    if (fd == f.sentinel)
        ++f.sentinel_closes;
    const bool directory = f.directory_open && fd == f.directory_fd;
    if (!directory && !(f.file_open && fd == f.file))
        return __real_close(fd);
    const auto op = directory ? Operation::directory_close : Operation::file_close;
    f.record(op);
    const int result = __real_close(fd);
    (directory ? f.directory_open : f.file_open) = false;
    // Linux releases the descriptor even when close reports a later I/O error.
    if (f.fails(op) || f.cleanup_close_error) {
        if (f.reuse_closed)
            f.sentinel = __real_open("/dev/null", O_RDONLY | O_CLOEXEC);
        return failure(f.fails(op) ? f.error : EBADF);
    }
    return result;
}
extern "C" int __wrap_rename(const char* from, const char* to) {
    if (!active || std::strcmp(from, active->temporary.data()) != 0)
        return __real_rename(from, to);
    active->record(Operation::rename);
    return active->fails(Operation::rename) ? failure(active->error) : __real_rename(from, to);
}
extern "C" int __wrap_open(const char* path, int flags, ...) {
    mode_t mode{};
    if ((flags & O_CREAT) != 0 || (flags & O_TMPFILE) == O_TMPFILE) {
        va_list args;
        va_start(args, flags);
        mode = static_cast<mode_t>(va_arg(args, int));
        va_end(args);
    }
    if (!active || active->directory != path || (flags & O_DIRECTORY) == 0)
        return __real_open(path, flags, mode);
    auto& f = *active;
    f.record(Operation::directory_open);
    if (f.fails(Operation::directory_open))
        return failure(f.error);
    const int fd = __real_open(path, flags, mode);
    if (fd >= 0) {
        f.directory_fd = fd;
        f.directory_open = true;
    }
    return fd;
}
extern "C" int __wrap_unlink(const char* path) {
    if (!active || std::strcmp(path, active->temporary.data()) != 0)
        return __real_unlink(path);
    active->record(Operation::unlink);
    return active->cleanup_unlink_error ? failure(EACCES) : __real_unlink(path);
}

namespace {
TEST(short_and_interrupted_writes_publish_complete_payload_in_order) {
    Temporary tmp;
    AssetRoot root(tmp.path);
    root.write_text("save", old_text);
    Faults f{.directory = root.directory().string(), .fragment = true};
    {
        Injection injection(f);
        root.write_atomic("save", bytes(new_text), observe);
        closed(f);
    }
    CHECK(root.text("save") == new_text && tmp.entries() == 1);
    CHECK(f.writes > 3 && f.events[0] == Operation::create);
    for (std::size_t i = 1; i <= f.writes; ++i)
        CHECK(f.events[i] == Operation::write);
    const std::array tail{Operation::file_sync, Operation::file_close, Operation::file_observer,
                          Operation::rename, Operation::rename_observer, Operation::directory_open,
                          Operation::directory_sync, Operation::directory_close};
    CHECK(f.event_count == 1 + f.writes + tail.size());
    CHECK(std::equal(tail.begin(), tail.end(), f.events.begin() +
                     static_cast<std::ptrdiff_t>(1 + f.writes)));
    struct stat info{};
    CHECK(stat((tmp.path / "save").c_str(), &info) == 0);
    CHECK((info.st_mode & 0777) == 0600);
}
TEST(empty_payload_is_synced_and_replaces_existing_file) {
    Temporary tmp;
    AssetRoot root(tmp.path);
    root.write_text("save", old_text);
    Faults f{.directory = root.directory().string()};
    {
        Injection injection(f);
        root.write_atomic("save", {});
        closed(f);
    }
    CHECK(f.writes == 0 && root.read("save", 0).empty() && tmp.entries() == 1);
    CHECK(std::count(f.events.begin(), f.events.begin() +
                     static_cast<std::ptrdiff_t>(f.event_count), Operation::file_sync) == 1);
}
TEST(pre_rename_faults_preserve_old_or_absent_destination_and_allow_retry) {
    struct Case { Operation op; int code; const char* diagnostic; };
    const std::array cases{
        Case{Operation::create, EACCES, "Create save temporary"},
        Case{Operation::create, EROFS, "Create save temporary"},
        Case{Operation::write, ENOSPC, "Write save temporary"},
        Case{Operation::write, EDQUOT, "Write save temporary"},
        Case{Operation::file_sync, EIO, "Sync save temporary"},
        Case{Operation::file_sync, EINTR, "Sync save temporary"},
        Case{Operation::file_close, EIO, "Close save temporary"},
        Case{Operation::file_close, EINTR, "Close save temporary"},
        Case{Operation::rename, EACCES, "Replace saved file"},
        Case{Operation::rename, EXDEV, "Replace saved file"}};
    for (bool existed : {false, true})
        for (const auto& c : cases) {
            Temporary tmp;
            AssetRoot root(tmp.path);
            if (existed)
                root.write_text("save", old_text);
            Faults f{.directory = root.directory().string(), .fail = c.op, .error = c.code};
            {
                Injection injection(f);
                expect_error([&] { root.write_atomic("save", bytes(new_text)); }, c.code, c.diagnostic);
                closed(f);
            }
            CHECK(tmp.entries() == (existed ? 1U : 0U));
            CHECK(existed ? root.text("save") == old_text : !std::filesystem::exists(tmp.path / "save"));
            root.write_text("save", new_text);
            CHECK(root.text("save") == new_text && tmp.entries() == 1);
        }
}
TEST(zero_progress_write_reports_io_error_instead_of_stale_errno) {
    for (int stale : {0, EACCES}) {
        Temporary tmp;
        AssetRoot root(tmp.path);
        root.write_text("save", old_text);
        Faults f{.directory = root.directory().string(), .fail = Operation::write,
                 .error = stale, .zero_write = true};
        {
            Injection injection(f);
            expect_error([&] { root.write_atomic("save", bytes(new_text)); }, EIO, "Write save temporary");
            closed(f);
            CHECK(f.writes == 1);
        }
        CHECK(root.text("save") == old_text && tmp.entries() == 1);
    }
}
TEST(post_rename_faults_report_error_but_keep_complete_new_destination) {
    struct Case { Operation op; int code; const char* diagnostic; };
    const std::array cases{Case{Operation::directory_open, EACCES, "Open save directory"},
                           Case{Operation::directory_sync, EIO, "Sync save directory"},
                           Case{Operation::directory_sync, EINTR, "Sync save directory"},
                           Case{Operation::directory_close, EIO, "Close save directory"},
                           Case{Operation::directory_close, EINTR, "Close save directory"}};
    for (bool existed : {false, true})
        for (const auto& c : cases) {
            Temporary tmp;
            AssetRoot root(tmp.path);
            if (existed)
                root.write_text("save", old_text);
            Faults f{.directory = root.directory().string(), .fail = c.op, .error = c.code};
            {
                Injection injection(f);
                expect_error([&] { root.write_atomic("save", bytes(new_text)); }, c.code, c.diagnostic);
                closed(f);
            }
            CHECK(root.text("save") == new_text && tmp.entries() == 1);
            root.write_text("save", old_text);
            CHECK(root.text("save") == old_text && tmp.entries() == 1);
        }
}
TEST(directory_sync_error_survives_close_and_cleanup_errors) {
    Temporary tmp;
    AssetRoot root(tmp.path);
    Faults f{.directory = root.directory().string(), .fail = Operation::directory_sync,
             .error = ENOSPC, .cleanup_unlink_error = true};
    // Activate the secondary close error only after the file was closed and renamed.
    {
        Injection injection(f);
        expect_error([&] {
            root.write_atomic("save", bytes(new_text), [](AtomicWriteStage stage) {
                if (stage == AtomicWriteStage::renamed)
                    active->cleanup_close_error = true;
            });
        }, ENOSPC, "Sync save directory");
        closed(f);
    }
    CHECK(root.text("save") == new_text && tmp.entries() == 1);
}
TEST(failed_cleanup_preserves_primary_error_and_orphan_does_not_block_retry) {
    Temporary tmp;
    AssetRoot root(tmp.path);
    root.write_text("save", old_text);
    Faults f{.directory = root.directory().string(), .fail = Operation::write,
             .error = ENOSPC, .cleanup_close_error = true, .cleanup_unlink_error = true};
    {
        Injection injection(f);
        expect_error([&] { root.write_atomic("save", bytes(new_text)); }, ENOSPC, "Write save temporary");
        closed(f);
    }
    CHECK(root.text("save") == old_text && tmp.entries() == 2);
    CHECK(std::filesystem::file_size(f.temporary.data()) == 5);
    root.write_text("save", new_text);
    CHECK(root.text("save") == new_text && tmp.entries() == 2);
    CHECK(std::filesystem::exists(f.temporary.data()));
}
TEST(failed_close_is_not_retried_after_descriptor_reuse) {
    for (auto op : {Operation::file_close, Operation::directory_close}) {
        Temporary tmp;
        AssetRoot root(tmp.path);
        Faults f{.directory = root.directory().string(), .fail = op,
                 .error = EINTR, .reuse_closed = true};
        Injection injection(f);
        expect_error([&] { root.write_atomic("save", bytes(new_text)); }, EINTR,
                     op == Operation::file_close ? "Close save temporary" : "Close save directory");
        closed(f);
        CHECK(f.sentinel == (op == Operation::file_close ? f.file : f.directory_fd));
        CHECK(fcntl(f.sentinel, F_GETFD) >= 0);
        CHECK(tmp.entries() == (op == Operation::file_close ? 0U : 1U));
    }
}
} // namespace
int main() { return testing::run_tests(); }
