#pragma once

// Virtual electronic pet layered on top of the GrokBot animation character.
// A Tamagotchi-style care simulation: six 0..100 vital stats, real-time decay,
// sleep, growth stages, exploration events, anti-spam petting, idle antics,
// levels and achievements. All state is persisted to NVS and offline decay is
// applied on boot. SuggestEmotion() maps the state to one of the 28 GrokBot
// atlas clips; the board feeds interactions from touch zones / BOOT clicks.

#include <atomic>
#include <cstdint>
#include <string>

class PetState {
public:
    static PetState& GetInstance();

    // Load persisted stats and apply the decay that happened while powered off.
    void Initialize();

    // ---- Interactions. Each returns a short Chinese notification. --------
    const char* Feed();      // +satiety, +mood, small health bonus
    const char* Play();      // +mood/fun, -energy, -satiety; needs energy
    const char* Pet();       // caress: +mood, anti-spam cooldown after 3 pets
    const char* Clean();     // +hygiene, +mood
    const char* Explore();   // random event: reward or mild penalty
    const char* Sleep();     // force sleep
    const char* Wake();      // wake up
    const char* Status();    // human-readable multi-line status page text

    // Periodic decay, call from a low-frequency task (roughly once a minute).
    void Tick();

    // Suggest an atlas emotion clip based on current stats and recent action.
    const char* SuggestEmotion() const;

    // Idle antics: occasionally suggest a "story" clip when all needs are met
    // and the pet has been left alone for a while. Returns nullptr normally.
    const char* IdleAntic() const;

    int mood() const { return mood_.load(); }
    int satiety() const { return satiety_.load(); }
    int energy() const { return energy_.load(); }
    int health() const { return health_.load(); }
    int hygiene() const { return hygiene_.load(); }
    int fun() const { return fun_.load(); }
    bool sleeping() const { return sleeping_.load(); }
    int level() const { return level_.load(); }
    int bonds() const { return bonds_.load(); }       // total interactions
    const char* stageName() const;

    // Human-readable one-line status, e.g. for notifications.
    std::string Describe() const;

private:
    PetState() = default;

    void Save() const;
    void Clamp();
    void CheckGrowth();
    void CheckAchievements();
    void MarkAction(int action, const char* emotion);

    std::atomic<int> mood_{70};
    std::atomic<int> satiety_{70};
    std::atomic<int> energy_{70};
    std::atomic<int> health_{80};
    std::atomic<int> hygiene_{70};
    std::atomic<int> fun_{65};
    std::atomic<int> bonds_{0};          // lifetime interaction count
    std::atomic<int> level_{1};
    std::atomic<int> growth_minutes_{0}; // powered-on minutes for growth stage
    std::atomic<int> pet_streak_{0};     // consecutive pets within spam window
    std::atomic<int> achievements_{0};   // bitmask

    std::atomic<bool> sleeping_{false};
    std::atomic<int64_t> last_update_us_{0};   // esp_timer time of last decay
    std::atomic<int64_t> last_action_us_{0};   // last interaction for transient emotion
    std::atomic<int> last_action_{0};          // emotion id for transient action
    std::atomic<int64_t> last_pet_us_{0};      // for anti-spam petting
    std::atomic<int64_t> last_interact_us_{0}; // for idle antics / auto sleep
};
