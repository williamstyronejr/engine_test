#include "engine/asset_watch.hpp"
#include "test.hpp"
#include <filesystem>
#include <fstream>
#include <unistd.h>

namespace {
using namespace engine;
using namespace testing;
using namespace std::chrono_literals;
using Time = AssetWatcher::Time;
struct Files {
    std::filesystem::path path;
    Files() {
        std::string pattern =
            (std::filesystem::temp_directory_path() / "asset-watch-XXXXXX").string();
        CHECK(mkdtemp(pattern.data()));
        path = pattern;
        std::filesystem::create_directory(path / "shaders");
        std::filesystem::create_directory(path / "textures");
        write("shaders/test.vert", "vertex");
        write("textures/test.etex", "texture");
    }
    ~Files() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
    void write(std::string_view key, std::string_view text) const {
        std::ofstream out(path / key);
        out << text;
        CHECK(out.good());
    }
};
const std::array watched{WatchedAsset{"shaders/test.vert", 1},
                         WatchedAsset{"textures/test.etex", 2}};
void start(AssetWatcher& watch) {
    CHECK(watch.poll(Time{}) == 0);
    CHECK(watch.poll(Time{} + 150ms) == 3);
    CHECK(watch.poll(Time{} + 200ms) == 0);
}
std::size_t descriptors() {
    std::size_t count = 0;
    for ([[maybe_unused]] const auto& entry : std::filesystem::directory_iterator("/proc/self/fd"))
        ++count;
    return count;
}
TEST(watch_configuration_limits_and_descriptor_lifetime) {
    Files files;
    AssetRoot root(files.path);
    rejects([&] { AssetWatcher w(root, {}); });
    for (const auto key : {"../escape", "/absolute", "shaders//bad", "shaders/../bad"})
        rejects([&] { AssetWatcher w(root, std::array{WatchedAsset{key, 1}}); });
    rejects([&] { AssetWatcher w(root, std::array{WatchedAsset{"file", 0}}); });
    rejects([&] { AssetWatcher w(root, std::array{watched[0], watched[0]}); });
    std::vector<WatchedAsset> keys;
    for (std::size_t i = 0; i <= AssetWatcher::max_files; ++i)
        keys.push_back({"missing" + std::to_string(i), 1});
    rejects([&] { AssetWatcher w(root, keys); });
    keys.pop_back();
    const auto baseline = descriptors();
    for (int cycle = 0; cycle < 20; ++cycle) {
        AssetWatcher watch(root, keys);
        CHECK(watch.size() == AssetWatcher::max_files && watch.notifications_active());
        CHECK(watch.poll(Time{}) == 0);
        CHECK(watch.poll(Time{} + 150ms) == 1);
    }
    CHECK(descriptors() == baseline);
}
TEST(startup_group_debounce_and_unrelated_files) {
    Files files;
    AssetWatcher watch(AssetRoot(files.path), watched);
    CHECK(watch.notifications_active());
    start(watch);
    files.write("textures/unrelated.etex", "ignored");
    CHECK(watch.poll(Time{} + 250ms) == 0);
    CHECK(watch.poll(Time{} + 450ms) == 0);
    files.write("shaders/test.vert", "first edit");
    CHECK(watch.poll(Time{} + 500ms) == 0);
    files.write("shaders/test.vert", "second edit");
    CHECK(watch.poll(Time{} + 600ms) == 0);
    CHECK(watch.poll(Time{} + 650ms) == 0);
    CHECK(watch.poll(Time{} + 750ms) == 1);
    CHECK(watch.poll(Time{} + 2s) == 0); // Periodic scan uses the updated baseline.
    rejects([&] { watch.poll(Time{} + 1s); });
}
TEST(atomic_saves_and_independent_group_deadlines) {
    Files files;
    AssetRoot root(files.path);
    AssetWatcher watch(root, watched);
    start(watch);
    root.write_text("shaders/test.vert", "atomically renamed vertex");
    CHECK(watch.poll(Time{} + 300ms) == 0);
    root.write_text("textures/test.etex", "atomically renamed texture");
    CHECK(watch.poll(Time{} + 400ms) == 0);
    CHECK(watch.poll(Time{} + 450ms) == 1);
    CHECK(watch.poll(Time{} + 550ms) == 2);
    CHECK(watch.poll(Time{} + 2s) == 0);
}
TEST(deleted_files_report_once_and_recreation_recovers) {
    Files files;
    AssetWatcher watch(AssetRoot(files.path), watched);
    start(watch);
    std::filesystem::remove(files.path / "textures/test.etex");
    CHECK(watch.poll(Time{} + 300ms) == 0);
    CHECK(watch.poll(Time{} + 450ms) == 2);
    CHECK(watch.poll(Time{} + 2s) == 0);
    files.write("textures/test.etex", "recreated");
    CHECK(watch.poll(Time{} + 2100ms) == 0);
    CHECK(watch.poll(Time{} + 2250ms) == 2);
}
TEST(directory_loss_falls_back_and_rearms_after_recreation) {
    Files files;
    AssetWatcher watch(AssetRoot(files.path), watched);
    start(watch);
    std::filesystem::rename(files.path / "textures", files.path / "old-textures");
    CHECK(watch.poll(Time{} + 300ms) == 0);
    CHECK(!watch.notifications_active());
    CHECK((watch.poll(Time{} + 450ms) & 2) == 2);
    std::filesystem::create_directory(files.path / "textures");
    files.write("textures/test.etex", "new directory");
    CHECK(watch.poll(Time{} + 1400ms) == 0);
    CHECK(watch.notifications_active());
    CHECK(watch.poll(Time{} + 1550ms) == 2);
    files.write("textures/test.etex", "watch is attached to the new inode");
    CHECK(watch.poll(Time{} + 1700ms) == 0);
    CHECK(watch.poll(Time{} + 1850ms) == 2);
}
TEST(metadata_recovers_alias_writes_and_ignores_escaped_symlink_targets) {
    Files files, external;
    std::filesystem::create_directory(files.path / "targets");
    files.write("targets/source", "initial");
    std::filesystem::remove(files.path / "textures/test.etex");
    std::filesystem::create_symlink("../targets/source", files.path / "textures/test.etex");
    AssetWatcher watch(AssetRoot(files.path), watched);
    start(watch);
    files.write("targets/source", "written through an unwatched pathname");
    CHECK(watch.poll(Time{} + 1100ms) == 0);
    CHECK(watch.poll(Time{} + 1250ms) == 2);
    std::filesystem::remove(files.path / "textures/test.etex");
    std::filesystem::create_symlink(external.path / "textures/test.etex",
                                    files.path / "textures/test.etex");
    CHECK(watch.poll(Time{} + 1400ms) == 0);
    CHECK(watch.poll(Time{} + 1550ms) == 2);
    external.write("textures/test.etex", "outside the asset root");
    CHECK(watch.poll(Time{} + 3s) == 0);
    CHECK(watch.poll(Time{} + 3200ms) == 0);
}
TEST(replacing_the_asset_root_reattaches_unchanged_child_paths) {
    Files files;
    const auto root_path = files.path / "watched";
    std::filesystem::create_directories(root_path / "shaders");
    std::filesystem::create_directories(root_path / "textures");
    files.write("watched/shaders/test.vert", "old vertex");
    files.write("watched/textures/test.etex", "old texture");
    AssetWatcher watch(AssetRoot(root_path), watched);
    start(watch);
    std::filesystem::rename(root_path, files.path / "retired");
    std::filesystem::create_directories(root_path / "shaders");
    std::filesystem::create_directories(root_path / "textures");
    files.write("watched/shaders/test.vert", "new vertex");
    files.write("watched/textures/test.etex", "new texture");
    CHECK(watch.poll(Time{} + 2s) == 0);
    CHECK(watch.poll(Time{} + 2150ms) == 3);
    CHECK(watch.notifications_active());
    files.write("watched/shaders/test.vert", "after reattachment");
    CHECK(watch.poll(Time{} + 2300ms) == 0);
    CHECK(watch.poll(Time{} + 2450ms) == 1);
}
TEST(saturated_event_budget_resynchronizes_and_explicit_rescan_is_debounced) {
    Files files;
    AssetWatcher watch(AssetRoot(files.path), watched);
    start(watch);
    const auto before = watch.recoveries();
    // Distinct names prevent inotify coalescing; unrelated traffic also consumes
    // the bounded read budget and must not leave watched changes stuck behind it.
    for (int i = 0; i < 700; ++i)
        files.write("textures/noise" + std::to_string(i), "noise");
    files.write("shaders/test.vert", "after flood");
    CHECK(watch.poll(Time{} + 300ms) == 0);
    CHECK(watch.recoveries() > before);
    CHECK(watch.poll(Time{} + 450ms) == 3);
    CHECK(watch.poll(Time{} + 2s) == 0);
    watch.request_rescan();
    CHECK(watch.poll(Time{} + 2100ms) == 0);
    CHECK(watch.poll(Time{} + 2250ms) == 3);
}
TEST(high_group_bit_and_initially_missing_directories) {
    Files files;
    constexpr std::uint32_t high = std::uint32_t{1} << 31;
    AssetWatcher watch(AssetRoot(files.path), std::array{WatchedAsset{"new/file", high}});
    CHECK(!watch.notifications_active());
    CHECK(watch.poll(Time{}) == 0);
    CHECK(watch.poll(Time{} + 150ms) == high);
    CHECK(watch.poll(Time{} + 1s) == 0);
    std::filesystem::create_directory(files.path / "new");
    files.write("new/file", "created");
    CHECK(watch.poll(Time{} + 2s) == 0);
    CHECK(watch.notifications_active());
    CHECK(watch.poll(Time{} + 2150ms) == high);
}
} // namespace
int main() {
    return testing::run_tests();
}
