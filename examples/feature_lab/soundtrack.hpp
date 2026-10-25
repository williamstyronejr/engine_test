#pragma once
#include "engine/audio.hpp"
#include "game.hpp"
#include "hud_text.hpp"
#include "settings.hpp"
#include <iostream>
#include <limits>

namespace feature_lab {
class Soundtrack {
  public:
    Soundtrack(const engine::AssetRoot& assets, bool enabled, std::string_view device = "default",
               AudioSettings initial = {}, bool initially_paused = false)
        : settings_(initial) {
        for (const float gain : {initial.master, initial.music, initial.effects})
            if (!std::isfinite(gain) || gain < 0 || gain > 1)
                throw std::invalid_argument("Audio setting outside [0,1]");
        if (!enabled)
            return;
        try {
            output_ = std::make_unique<engine::AudioOutput>(
                device, engine::MixerGains{settings_.muted ? 0 : settings_.master, settings_.music,
                                           settings_.effects});
            for (std::size_t i = 0; i < keys.size(); ++i)
                sounds_[i] = output_->register_sound(engine::decode_wav(
                    assets.read(keys[i], engine::SoundData::max_file_bytes), keys[i]));
            if (initially_paused) {
                require(output_->pause_group(engine::AudioGroup::music, true));
                require(output_->pause_group(engine::AudioGroup::effects, true));
                paused_ = true;
            }
            start_loops();
        } catch (const std::exception& error) {
            disable(error.what());
        }
    }
    void update(const Game& game, const engine::InputFrame& controls, int picked) {
        using namespace engine;
        if (!output_)
            return;
        try {
            if (!output_->healthy())
                throw std::runtime_error("Audio device failed");
            if (button(controls, Key::mute).pressed) {
                settings_.muted = !settings_.muted;
                require(output_->set_master_gain(settings_.muted ? 0 : settings_.master));
            }
            if (button(controls, Key::music_volume).pressed) {
                music_level_ = (music_level_ + 1) % music_levels.size();
                settings_.music = music_levels[music_level_];
                require(output_->set_group_gain(AudioGroup::music, settings_.music));
            }
            if (button(controls, Key::effects_volume).pressed) {
                effect_level_ = (effect_level_ + 1) % effect_levels.size();
                settings_.effects = effect_levels[effect_level_];
                require(output_->set_group_gain(AudioGroup::effects, settings_.effects));
            }
            if (button(controls, Key::restart).pressed) {
                require(output_->stop_all());
                start_loops();
                door_ = won_ = false;
            }
            if (game.paused != paused_) {
                require(output_->pause_group(AudioGroup::music, game.paused));
                require(output_->pause_group(AudioGroup::effects, game.paused));
                paused_ = game.paused;
            }
            for (int i = 0; i < picked; ++i)
                require(static_cast<bool>(output_->play(
                    sounds_[0], {0.65F, std::clamp(game.position.x / 32, -1.0F, 1.0F)})));
            if (game.door_started && !door_) {
                door_ = true;
                require(static_cast<bool>(output_->play(
                    sounds_[3],
                    {0.6F, std::clamp((game.location(game.exit).x - game.position.x) / 12, -1.0F,
                                      1.0F)})));
            }
            if (game.won && !won_) {
                won_ = true;
                require(output_->stop(machine_));
            }
            if (!game.won && !game.paused) {
                const auto offset = emitter_ - game.position;
                const float gain = 0.4F / (1 + std::hypot(offset.x, offset.y) * 0.3F);
                const float pan = std::clamp(offset.x / 12, -1.0F, 1.0F);
                if (std::abs(gain - last_gain_) > 0.005F) {
                    require(output_->set_gain(machine_, gain));
                    last_gain_ = gain;
                }
                if (std::abs(pan - last_pan_) > 0.02F) {
                    require(output_->set_pan(machine_, pan));
                    last_pan_ = pan;
                }
            }
        } catch (const std::exception& error) {
            disable(error.what());
        }
    }
    void restored(const Game& game) {
        if (!output_)
            return;
        try {
            require(output_->stop_all());
            start_loops();
            door_ = game.door_started;
            won_ = false;
            paused_ = !game.paused; // Force group pause state to match the loaded game.
            locate_emitter(game);
            update(game, {}, 0); // No replay of old pickup/door one-shots.
        } catch (const std::exception& error) {
            disable(error.what());
        }
    }
    void locate_emitter(const Game& game) {
        emitter_ = {};
        game.world.each([&](const engine::SceneNode& node) {
            if (node.tag == "machine")
                emitter_ = game.location(node.entity);
        });
    }
    // Audio is presentation-only, so this is also available during replay.
    // Decode the whole bank before queuing one transaction; never disable working
    // output because an edited file or a replacement command was rejected.
    bool reload(const engine::AssetRoot& assets) {
        if (!output_)
            return false;
        try {
            if (revision_ == std::numeric_limits<std::uint64_t>::max())
                throw std::runtime_error("Audio bank revision exhausted");
            output_->collect_sounds();
            std::vector<engine::SoundReplacement> batch;
            batch.reserve(keys.size());
            std::size_t bytes = 0;
            for (std::size_t i = 0; i < keys.size(); ++i) {
                auto data = engine::decode_wav(
                    assets.read(keys[i], engine::SoundData::max_file_bytes), keys[i]);
                bytes += data.samples.size() * sizeof(float);
                if (bytes > engine::AudioOutput::max_sound_bytes - output_->retained_sound_bytes())
                    throw std::runtime_error("Audio reload exceeds retained sample budget");
                batch.push_back({sounds_[i], std::move(data)});
            }
            if (!output_->replace_sounds(batch))
                throw std::runtime_error("Audio replacement command rejected");
            ++revision_;
            reload_applied_ = true;
            reload_diagnostic_.clear();
        } catch (const std::exception& error) {
            reload_applied_ = false;
            reload_diagnostic_ = error.what();
        }
        std::cerr << "[audio bank " << (reload_applied_ ? "queued" : "retained")
                  << "] revision=" << revision_ << std::endl;
        if (!reload_diagnostic_.empty())
            std::cerr << reload_diagnostic_ << '\n';
        return reload_applied_;
    }
    void collect() {
        if (output_)
            output_->collect_sounds();
    }
    std::uint64_t revision() const { return revision_; }
    bool reload_applied() const { return reload_applied_; }
    const std::string& reload_diagnostic() const { return reload_diagnostic_; }
    std::size_t sound_bytes() const { return output_ ? output_->sound_bytes() : 0; }
    std::size_t retained_sound_bytes() const {
        return output_ ? output_->retained_sound_bytes() : 0;
    }
    std::string_view reload_status(std::string& output) const {
        if (!output_)
            return format_text(output, "AUDIO BANK OFF");
        if (reload_applied_ && revision_ == 1)
            return format_text(output, "AUDIO BANK READY R1");
        return format_text(output,
                           reload_applied_ ? "AUDIO BANK QUEUED R" : "AUDIO BANK FAILED - KEPT R",
                           revision_);
    }
    std::string reload_status() const {
        std::string output;
        reload_status(output);
        return output;
    }
    std::string_view status(std::string& output, const engine::KeyBindings& bindings = {}) const {
        if (!output_)
            return format_text(output, "AUDIO OFF");
        return format_text(output, bindings.key_name(engine::Key::mute),
                           settings_.muted ? " MUTED" : " SOUND ON", " / ",
                           bindings.key_name(engine::Key::music_volume), " MUSIC ",
                           static_cast<int>(settings_.music * 100), " / ",
                           bindings.key_name(engine::Key::effects_volume), " EFFECTS ",
                           static_cast<int>(settings_.effects * 100));
    }
    std::string status(const engine::KeyBindings& bindings = {}) const {
        std::string output;
        status(output, bindings);
        return output;
    }
    AudioSettings settings() const { return settings_; }
    void configure(AudioSettings value) {
        for (const auto gain : {value.master, value.music, value.effects})
            if (!std::isfinite(gain) || gain < 0 || gain > 1)
                throw std::invalid_argument("Audio setting outside [0,1]");
        if (!output_) {
            settings_ = value;
            return;
        }
        try {
            if (value.master != settings_.master || value.muted != settings_.muted)
                require(output_->set_master_gain(value.muted ? 0 : value.master));
            if (value.music != settings_.music)
                require(output_->set_group_gain(engine::AudioGroup::music, value.music));
            if (value.effects != settings_.effects)
                require(output_->set_group_gain(engine::AudioGroup::effects, value.effects));
            settings_ = value;
        } catch (const std::exception& error) {
            disable(error.what());
        }
    }
    engine::AudioStats stats() const { return output_ ? output_->stats() : final_stats_; }
    bool enabled() const { return static_cast<bool>(output_); }
    static constexpr std::array<std::string_view, 4> keys = {
        "sounds/pickup.wav", "sounds/music.wav", "sounds/machine.wav", "sounds/door.wav"};

  private:
    void require(bool accepted) {
        if (!accepted)
            throw std::runtime_error("Audio command rejected; playback disabled");
    }
    void start_loops() {
        using namespace engine;
        require(static_cast<bool>(output_->play(sounds_[1], {1, 0, true, AudioGroup::music})));
        machine_ = output_->play(sounds_[2], {0, 0, true, AudioGroup::effects});
        require(static_cast<bool>(machine_));
        last_gain_ = 0;
        last_pan_ = 0;
    }
    void disable(std::string_view reason) {
        if (output_)
            final_stats_ = output_->stats();
        std::cerr << "[audio] Muted: " << reason;
        if (final_stats_.device_error)
            std::cerr << " (device_error=" << final_stats_.device_error << ')';
        std::cerr << '\n';
        output_.reset();
    }
    static constexpr std::array<float, 3> music_levels{0.3F, 0.15F, 0},
        effect_levels{0.5F, 0.25F, 0};
    std::unique_ptr<engine::AudioOutput> output_;
    std::array<engine::SoundHandle, 4> sounds_{};
    engine::VoiceHandle machine_;
    engine::Vec2 emitter_{0, 9};
    std::size_t music_level_{}, effect_level_{};
    float last_gain_{}, last_pan_{};
    AudioSettings settings_;
    engine::AudioStats final_stats_;
    bool paused_{}, door_{}, won_{};
    std::uint64_t revision_{1};
    bool reload_applied_{true};
    std::string reload_diagnostic_;
};
} // namespace feature_lab
