#include "engine/persistence.hpp"
#include "test.hpp"
#include <array>
#include <cerrno>
#include <cstdarg>
#include <cstdlib>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>

extern "C" {
int __real_mkdir(const char*, mode_t);
int __real_open(const char*, int, ...);
int __real_fsync(int);
int __real_close(int);
int __real_mkstemp(char*);
}
namespace {
using namespace engine;
using namespace testing;
enum class Operation { none, mkdir, open, sync, close };
enum class Race { none, directory, file };
struct Trace {
    std::vector<std::filesystem::path> chain;
    Operation fault{Operation::none};
    Race race{Race::none};
    std::size_t index{}, fd_index{}, mkdir_calls{}, opens{}, closes{}, synced{}, publications{};
    int error{EIO}, fd{-1}, sentinel{-1}, reused_from{-1};
    bool writing{}, early_publication{}, bad_order{}, hit{}, race_done{};
    bool secondary_close_error{}, reuse_closed{}, crash{};
    unsigned sentinel_closes{};
    explicit Trace(const std::filesystem::path& leaf) {
        for (auto p = std::filesystem::weakly_canonical(leaf);; p = p.parent_path()) {
            chain.push_back(p);
            if (p == p.root_path())
                break;
        }
    }
    std::size_t locate(const char* path) const {
        for (std::size_t i = 0; i < chain.size(); ++i)
            if (chain[i].native() == path)
                return i;
        return chain.size();
    }
    bool fails(Operation op, std::size_t at) {
        if (fault != op || at != index)
            return false;
        hit = true;
        return true;
    }
};
Trace* active{};
int fail(int error) { errno = error; return -1; }
struct Injection {
    Trace& trace;
    explicit Injection(Trace& t) : trace(t) { CHECK(!active); active = &t; }
    ~Injection() {
        active = nullptr;
        if (trace.fd >= 0)
            __real_close(trace.fd);
        if (trace.sentinel >= 0)
            __real_close(trace.sentinel);
    }
};
struct Temporary {
    std::filesystem::path path;
    Temporary() {
        std::string pattern =
            (std::filesystem::temp_directory_path() / "engine-directory-fault-XXXXXX").string();
        CHECK(mkdtemp(pattern.data()));
        path = std::filesystem::canonical(pattern);
    }
    ~Temporary() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
    UserPaths paths() const { return {path / "new/parent/config", path / "new/parent/state"}; }
};
auto record(std::uint8_t value) {
    return seal_record({'T', 'E', 'S', 'T', 1, 0, 0, value});
}
void save(const UserPaths& paths, bool config, const std::vector<std::uint8_t>& bytes) {
    const UserStorage storage(paths); // Reconstruct on every attempt; no per-object cache.
    if (config)
        storage.write_config(bytes);
    else
        storage.write_slot(2, bytes);
}
auto load(const UserPaths& paths, bool config) {
    const UserStorage storage(paths);
    return config ? storage.read_config() : storage.read_slot(2);
}
void released(const Trace& trace) {
    CHECK(trace.fd < 0 && trace.opens == trace.closes);
    CHECK(trace.sentinel_closes == 0 && !trace.bad_order);
}
void prepared(const Trace& trace) {
    released(trace);
    CHECK(!trace.early_publication && trace.publications == 1);
    CHECK(trace.synced == trace.chain.size() && trace.closes == trace.chain.size());
}
void no_temporary_files(const std::filesystem::path& root) {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
        CHECK(!entry.path().filename().string().starts_with(".engine-save-"));
}
template <class F> void expected_error(F function, int code, std::string_view operation,
                                     const std::filesystem::path& path) {
    bool caught = false;
    try { function(); }
    catch (const std::system_error& e) {
        CHECK(e.code() == std::error_code(code, std::generic_category()));
        CHECK(std::string_view(e.what()).find(operation) != std::string_view::npos);
        CHECK(std::string_view(e.what()).find(path.string()) != std::string_view::npos);
        caught = true;
    }
    CHECK(caught);
}
} // namespace

extern "C" int __wrap_mkdir(const char* path, mode_t mode) {
    if (!active || active->writing)
        return __real_mkdir(path, mode);
    auto& t = *active;
    const auto i = t.locate(path);
    if (i == t.chain.size())
        return __real_mkdir(path, mode);
    ++t.mkdir_calls;
    if (t.fails(Operation::mkdir, i))
        return fail(t.error);
    if (t.race != Race::none && i == t.index && !t.race_done) {
        t.race_done = true;
        if (t.race == Race::directory) {
            if (__real_mkdir(path, mode) != 0)
                return -1;
        } else {
            const int fd = __real_open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
            if (fd < 0)
                return -1;
            __real_close(fd);
        }
        return fail(EEXIST);
    }
    return __real_mkdir(path, mode);
}
extern "C" int __wrap_open(const char* path, int flags, ...) {
    mode_t mode{};
    if ((flags & O_CREAT) != 0 || (flags & O_TMPFILE) == O_TMPFILE) {
        va_list args;
        va_start(args, flags);
        mode = static_cast<mode_t>(va_arg(args, int));
        va_end(args);
    }
    if (!active || active->writing || (flags & O_DIRECTORY) == 0)
        return __real_open(path, flags, mode);
    auto& t = *active;
    const auto i = t.locate(path);
    if (i == t.chain.size())
        return __real_open(path, flags, mode);
    if (t.fails(Operation::open, i))
        return fail(t.error);
    const int fd = __real_open(path, flags, mode);
    if (fd >= 0) {
        if (t.fd >= 0 || i != t.synced)
            t.bad_order = true;
        t.fd = fd;
        t.fd_index = i;
        ++t.opens;
    }
    return fd;
}
extern "C" int __wrap_fsync(int fd) {
    if (!active || active->writing || fd != active->fd)
        return __real_fsync(fd);
    auto& t = *active;
    if (t.fails(Operation::sync, t.fd_index)) {
        if (t.crash)
            _exit(91);
        return fail(t.error);
    }
    const int result = __real_fsync(fd);
    if (result == 0)
        ++t.synced;
    return result;
}
extern "C" int __wrap_close(int fd) {
    if (!active || active->writing)
        return __real_close(fd);
    auto& t = *active;
    if (fd == t.sentinel)
        ++t.sentinel_closes;
    if (fd != t.fd)
        return __real_close(fd);
    const int result = __real_close(fd);
    t.fd = -1;
    ++t.closes;
    if (t.fails(Operation::close, t.fd_index) ||
        (t.secondary_close_error && t.fd_index == t.index)) {
        if (t.reuse_closed) {
            t.reused_from = fd;
            t.sentinel = __real_open("/dev/null", O_RDONLY | O_CLOEXEC);
        }
        return fail(t.fault == Operation::close ? t.error : EBADF);
    }
    return result;
}
extern "C" int __wrap_mkstemp(char* path) {
    if (!active)
        return __real_mkstemp(path);
    auto& t = *active;
    ++t.publications;
    t.early_publication = t.fd >= 0 || t.synced != t.chain.size() || t.closes != t.chain.size();
    t.writing = true;
    return __real_mkstemp(path);
}

namespace {
TEST(new_and_existing_config_and_slot_sync_every_ancestor_before_file_creation) {
    Temporary tmp;
    const auto paths = tmp.paths();
    for (bool config : {true, false})
        for (auto value : {std::uint8_t{1}, std::uint8_t{2}}) {
            Trace trace(config ? paths.config : paths.state);
            {
                Injection injection(trace);
                save(paths, config, record(value));
                prepared(trace);
            }
            CHECK(load(paths, config) == record(value));
            no_temporary_files(tmp.path);
        }
}
TEST(mkdir_errors_leave_partial_tree_and_retry_prepares_it_again) {
    for (bool config : {true, false})
        for (int error : {EACCES, ENOSPC, EROFS}) {
            Temporary tmp;
            const auto paths = tmp.paths();
            const auto leaf = config ? paths.config : paths.state;
            Trace trace(leaf);
            trace.fault = Operation::mkdir;
            trace.index = 1; // Leave new/ behind, but reject new/parent/.
            trace.error = error;
            {
                Injection injection(trace);
                expected_error([&] { save(paths, config, record(1)); }, error,
                               "Create save directory", leaf.parent_path());
                CHECK(trace.hit && trace.publications == 0);
                released(trace);
            }
            CHECK(std::filesystem::is_directory(tmp.path / "new"));
            CHECK(!std::filesystem::exists(leaf) && !load(paths, config));
            Trace retry(leaf);
            { Injection injection(retry); save(paths, config, record(2)); prepared(retry); }
            CHECK(load(paths, config) == record(2));
            no_temporary_files(tmp.path);
        }
}
TEST(directory_open_sync_close_errors_abort_before_new_or_existing_record_changes) {
    struct Case { Operation op; int error; const char* diagnostic; };
    const std::array cases{Case{Operation::open, EACCES, "Open save directory chain"},
        Case{Operation::sync, EIO, "Sync save directory chain"},
        Case{Operation::sync, EINTR, "Sync save directory chain"},
        Case{Operation::sync, EINVAL, "Sync save directory chain"},
        Case{Operation::close, EIO, "Close save directory chain"},
        Case{Operation::close, EINTR, "Close save directory chain"}};
    for (bool existed : {false, true})
        for (const auto& c : cases)
            for (unsigned position : {0U, 1U, 2U}) {
                Temporary tmp;
                const auto paths = tmp.paths();
                if (existed)
                    save(paths, true, record(1));
                Trace trace(paths.config);
                trace.fault = c.op;
                trace.error = c.error;
                trace.index = position == 2 ? trace.chain.size() - 1 : position;
                {
                    Injection injection(trace);
                    expected_error([&] { save(paths, true, record(2)); }, c.error,
                                   c.diagnostic, trace.chain[trace.index]);
                    CHECK(trace.hit && trace.publications == 0);
                    released(trace);
                }
                CHECK(existed ? load(paths, true) == record(1) : !load(paths, true));
                no_temporary_files(tmp.path);
                Trace retry(paths.config);
                { Injection injection(retry); save(paths, true, record(2)); prepared(retry); }
                CHECK(load(paths, true) == record(2));
            }
}
TEST(sync_error_precedes_close_error_and_failed_close_does_not_close_reused_fd) {
    for (auto operation : {Operation::sync, Operation::close}) {
        Temporary tmp;
        const auto paths = tmp.paths();
        Trace trace(paths.state);
        trace.fault = operation;
        trace.index = 1;
        trace.error = operation == Operation::sync ? ENOSPC : EINTR;
        trace.secondary_close_error = true;
        trace.reuse_closed = true;
        Injection injection(trace);
        expected_error([&] { save(paths, false, record(1)); }, trace.error,
                       operation == Operation::sync ? "Sync save directory chain" : "Close save directory chain",
                       trace.chain[trace.index]);
        CHECK(trace.hit && trace.publications == 0);
        released(trace);
        CHECK(trace.sentinel >= 0 && trace.sentinel == trace.reused_from);
        CHECK(fcntl(trace.sentinel, F_GETFD) >= 0);
        CHECK(!load(paths, false));
    }
}
TEST(existing_symlink_ancestors_sync_resolved_tree) {
    Temporary tmp;
    std::filesystem::create_directories(tmp.path / "actual/parent");
    std::filesystem::create_directory_symlink(tmp.path / "actual/parent", tmp.path / "alias");
    const UserPaths paths{tmp.path / "alias/config", tmp.path / "alias/state"};
    Trace trace(tmp.path / "actual/parent/config");
    { Injection injection(trace); save(paths, true, record(1)); prepared(trace); }
    CHECK(load(paths, true) == record(1));
    CHECK(std::filesystem::is_regular_file(tmp.path / "actual/parent/config/settings.ecfg"));
}
TEST(eexist_race_is_accepted_only_for_real_directories) {
    for (auto race : {Race::directory, Race::file}) {
        Temporary tmp;
        const auto paths = tmp.paths();
        Trace trace(paths.config);
        trace.race = race;
        {
            Injection injection(trace);
            if (race == Race::directory) {
                save(paths, true, record(1));
                prepared(trace);
            } else {
                expected_error([&] { save(paths, true, record(1)); }, ENOTDIR,
                               "Open save directory chain", paths.config);
                CHECK(trace.publications == 0);
                released(trace);
            }
            CHECK(trace.race_done);
        }
        if (race == Race::file) {
            CHECK(std::filesystem::is_regular_file(paths.config));
            CHECK(std::filesystem::file_size(paths.config) == 0);
            std::filesystem::remove(paths.config);
            Trace retry(paths.config);
            { Injection injection(retry); save(paths, true, record(2)); prepared(retry); }
        }
        CHECK(load(paths, true) == record(race == Race::directory ? 1 : 2));
    }
}
TEST(reads_invalid_envelopes_and_invalid_slots_have_no_creation_side_effects) {
    Temporary tmp;
    const auto paths = tmp.paths();
    Trace trace(paths.config);
    const UserStorage storage(paths);
    {
        Injection injection(trace);
        CHECK(!storage.read_config() && !storage.read_slot(1));
        rejects([&] { storage.write_config({}); });
        auto corrupt = record(1);
        corrupt.back() ^= 1;
        rejects([&] { storage.write_config(corrupt); });
        rejects([&] { storage.write_slot(0, record(1)); });
        CHECK(trace.mkdir_calls == 0 && trace.opens == 0 && trace.publications == 0);
        CHECK(std::filesystem::is_empty(tmp.path));
    }
    storage.write_config(record(1));
    Trace query(paths.config);
    Injection injection(query);
    CHECK(storage.read_config() == record(1));
    CHECK(query.mkdir_calls == 0 && query.opens == 0 && query.publications == 0);
}
TEST(existing_nondirectory_is_not_replaced_or_treated_as_prepared_storage) {
    Temporary tmp;
    const auto paths = tmp.paths();
    std::filesystem::create_directories(paths.config.parent_path());
    const AssetRoot parent(paths.config.parent_path());
    parent.write_text("config", "unrelated file contents");
    Trace trace(paths.config);
    {
        Injection injection(trace);
        expected_error([&] { save(paths, true, record(1)); }, ENOTDIR,
                       "Open save directory chain", paths.config);
        CHECK(trace.publications == 0);
        released(trace);
    }
    CHECK(parent.text("config") == "unrelated file contents");
    no_temporary_files(tmp.path);
}
TEST(process_interruption_during_preparation_does_not_make_retry_skip_existing_chain) {
    Temporary tmp;
    const auto paths = tmp.paths();
    const auto child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        Trace trace(paths.state);
        trace.fault = Operation::sync;
        trace.index = 1;
        trace.crash = true;
        Injection injection(trace);
        save(paths, false, record(1));
        _exit(92);
    }
    int status{};
    CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 91);
    CHECK(std::filesystem::is_directory(paths.state) && !load(paths, false));
    Trace retry(paths.state);
    { Injection injection(retry); save(paths, false, record(2)); prepared(retry); }
    CHECK(load(paths, false) == record(2));
    no_temporary_files(tmp.path);
}
} // namespace
int main() { return testing::run_tests(); }
