#pragma once
#include <iostream>

#include <algorithm>
#include <functional>
#include <memory>
#include <queue>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

struct DummyEntity {
    void* handle = reinterpret_cast<void*>(0x1);
    operator bool() const { return handle != nullptr; }
};
using Entity = DummyEntity;

namespace ptgn {

struct Subscription {
    std::type_index type{typeid(void)};
    std::size_t     slot = static_cast<std::size_t>(-1);
    Entity          entity{};
    bool            valid = false;
};

template <typename T>
class Channel {
public:
    struct Slot {
        Entity entity{};
        int priority = 0;         // higher runs first
        bool alive = false;
        bool once  = false;
        std::function<void(const T&)> fn;
        std::weak_ptr<void> track; // expires when script dies; empty for non-script handlers
    };

    Subscription subscribe(Entity e,
                           std::function<void(const T&)> fn,
                           int priority,
                           bool once,
                           std::weak_ptr<void> track = {}) {
        std::size_t idx;
        if (!free_.empty()) { idx = free_.back(); free_.pop_back(); slots_[idx] = Slot{}; }
        else { idx = slots_.size(); slots_.push_back(Slot{}); }

        auto& s = slots_[idx];
        s.entity = e; s.priority = priority; s.fn = std::move(fn);
        s.once = once; s.track = std::move(track);
        s.alive = true;
        dirty_order_ = true;
        return Subscription{ std::type_index(typeid(T)), idx, e, true };
    }

    void unsubscribe(const Subscription& sub) {
        if (!sub.valid || sub.type != std::type_index(typeid(T))) return;
        if (sub.slot >= slots_.size()) return;
        auto& s = slots_[sub.slot];
        if (s.alive && s.entity == sub.entity) { s.alive = false; free_.push_back(sub.slot); }
    }

    template <typename FEntityInvoker>
    void publish(const T& evt, FEntityInvoker&& invoke_for) {
        order_if_needed();
        for (std::size_t k = 0; k < order_.size(); ++k) {
            auto idx = order_[k];
            if (idx >= slots_.size()) continue;
            auto& s = slots_[idx];
            if (!s.alive) continue;

            // auto-mark dead if tracked owner expired
            if (!s.track.owner_before(std::weak_ptr<void>{}) && s.track.expired()) {
                s.alive = false; free_.push_back(idx); continue;
            }
            // auto-mark dead if entity invalid
            if (!s.entity) { s.alive = false; free_.push_back(idx); continue; }

            invoke_for(s.entity, s.fn, evt);

            if (s.once) { s.alive = false; free_.push_back(idx); }
        }
    }

    void gc() {
        if (free_.empty()) return;
        std::vector<Slot> compact; compact.reserve(slots_.size() - free_.size());
        for (auto& s : slots_) if (s.alive) compact.push_back(std::move(s));
        slots_.swap(compact);
        rebuild_order();
        free_.clear();
    }

private:
    std::vector<Slot> slots_;
    std::vector<std::size_t> free_;
    std::vector<std::size_t> order_;
    bool dirty_order_ = true;

    void order_if_needed() { if (dirty_order_) rebuild_order(); }
    void rebuild_order() {
        order_.clear(); order_.reserve(slots_.size());
        for (std::size_t i = 0; i < slots_.size(); ++i) if (slots_[i].alive) order_.push_back(i);
        std::sort(order_.begin(), order_.end(),
                  [&](std::size_t a, std::size_t b){ return slots_[a].priority > slots_[b].priority; });
        dirty_order_ = false;
    }
};

// Core event hub. Use one globally (in EngineContext) and one per Scene.
class EventHub {
public:
    // Generic (no tracking): free/static functions, global lambdas
    template <typename T, typename Fn>
    Subscription On(Entity e, Fn&& fn, int priority = 0) {
        return chan<T>().subscribe(
            e, std::function<void(const T&)>(std::forward<Fn>(fn)), priority, false, {}
        );
    }
    template <typename T, typename Fn>
    Subscription Once(Entity e, Fn&& fn, int priority = 0) {
        return chan<T>().subscribe(
            e, std::function<void(const T&)>(std::forward<Fn>(fn)), priority, true, {}
        );
    }

    // Script-bound (tracked) variants
    template <typename T, typename Fn>
    Subscription OnTracked(Entity e, Fn&& fn, int priority, std::weak_ptr<void> track) {
        return chan<T>().subscribe(
            e, std::function<void(const T&)>(std::forward<Fn>(fn)), priority, false, std::move(track)
        );
    }
    template <typename T, typename Fn>
    Subscription OnceTracked(Entity e, Fn&& fn, int priority, std::weak_ptr<void> track) {
        return chan<T>().subscribe(
            e, std::function<void(const T&)>(std::forward<Fn>(fn)), priority, true, std::move(track)
        );
    }

    // Emit / Post
    template <typename T>
    void Emit(const T& evt) {
        chan<T>().publish(evt, [&](Entity /*e*/, auto& fn, const T& ev){ fn(ev); });
    }

    // Queue a Emit when Pump is called.
    template <typename T>
    void Post(T evt) {
        queue_.emplace(Envelope{
            std::type_index(typeid(T)),
            std::make_shared<T>(std::move(evt)),
            [](EventHub& self, const std::shared_ptr<void>& p){
                self.Emit<T>(*static_cast<T*>(p.get()));
            }
        });
    }

    void Pump() {
        while (!queue_.empty()) {
            auto env = std::move(queue_.front()); queue_.pop();
            env.dispatch(*this, env.payload);
        }
    }

    void GC() { for (auto& [_, h] : map_) h->gc(); }

private:
    struct IHolder { virtual ~IHolder() = default; virtual void gc() = 0; };
    template <typename T> struct Holder : IHolder {
        Channel<T> ch; void gc() override { ch.gc(); }
    };

    template <typename T>
    Channel<T>& chan() {
        auto key = std::type_index(typeid(T));
        auto it = map_.find(key);
        if (it == map_.end()) {
            auto up = std::make_unique<Holder<T>>(); auto* raw = up.get();
            map_.emplace(key, std::move(up));
            return raw->ch;
        }
        return static_cast<Holder<T>*>(it->second.get())->ch;
    }

    struct Envelope {
        std::type_index type;
        std::shared_ptr<void> payload;
        void (*dispatch)(EventHub&, const std::shared_ptr<void>&);
    };

    std::unordered_map<std::type_index, std::unique_ptr<IHolder>> map_;
    std::queue<Envelope> queue_;
};

} // namespace ptgn

namespace ptgn {

struct EngineContext {
    EventHub& globalEvents;

    template <typename T>
    void Emit(const T& evt) { globalEvents.Emit<T>(evt); }

    template <typename T>
    void Post(T evt) { globalEvents.Post<T>(std::move(evt)); }

    void PumpEvents() { globalEvents.Pump(); }
    void GCEvents()   { globalEvents.GC(); }
};

} // namespace ptgn

namespace ptgn {

class Scene {
public:
    explicit Scene(EngineContext& ctx) : ctx_(&ctx) {}

    EngineContext& ctx() const { return *ctx_; }
    EventHub&      events()    { return sceneEvents_; }

    // Scene-local emit/post; called by scripts via ScriptBase::Emit(...)
    template <typename T> void Emit(const T& evt) { sceneEvents_.Emit<T>(evt); }
    template <typename T> void Post(T evt) { sceneEvents_.Post<T>(std::move(evt)); }
    void PumpEvents() { sceneEvents_.Pump(); }
    void GCEvents()   { sceneEvents_.GC();   }

private:
    EngineContext* ctx_{};
    EventHub sceneEvents_;
};

} // namespace ptgn

namespace ptgn {

// Base for all scripts. No EventSink passed; helpers live here.
// Engine sets ctx_ and scene_ when attaching the script.
class ScriptBase : public std::enable_shared_from_this<ScriptBase> {
public:
    virtual ~ScriptBase() = default;

    // Lifecycle
    virtual void OnCreate() {} // call OnGlobal/OnScene here

    // Access to context / scene
    EngineContext* ctx()   const { return ctx_; }
    Scene*         scene() const { return scene_; }
    Entity         entity()const { return entity_; }

protected:
    // ---- SUBSCRIBE HELPERS (tracked; auto-clean when script dies) ----
    template <typename T, typename C>
    Subscription OnGlobal(void (C::*mf)(const T&), int priority = 0) {
        static_assert(std::is_base_of_v<ScriptBase, C>);
        auto wp = this->weak_from_this();
        auto bound = [wp, mf](const T& ev){
            if (auto sp = wp.lock()) if (auto* obj = dynamic_cast<C*>(sp.get())) (obj->*mf)(ev);
        };
        return ctx_->globalEvents.OnTracked<T>(entity_, std::move(bound), priority, wp);
    }

    template <typename T, typename C>
    Subscription OnceGlobal(void (C::*mf)(const T&), int priority = 0) {
        static_assert(std::is_base_of_v<ScriptBase, C>);
        auto wp = this->weak_from_this();
        auto bound = [wp, mf](const T& ev){
            if (auto sp = wp.lock()) if (auto* obj = dynamic_cast<C*>(sp.get())) (obj->*mf)(ev);
        };
        return ctx_->globalEvents.OnceTracked<T>(entity_, std::move(bound), priority, wp);
    }

    template <typename T, typename C>
    Subscription OnScene(void (C::*mf)(const T&), int priority = 0) {
        static_assert(std::is_base_of_v<ScriptBase, C>);
        auto wp = this->weak_from_this();
        auto bound = [wp, mf](const T& ev){
            if (auto sp = wp.lock()) if (auto* obj = dynamic_cast<C*>(sp.get())) (obj->*mf)(ev);
        };
        return scene_->events().OnTracked<T>(entity_, std::move(bound), priority, wp);
    }

    template <typename T, typename C>
    Subscription OnceScene(void (C::*mf)(const T&), int priority = 0) {
        static_assert(std::is_base_of_v<ScriptBase, C>);
        auto wp = this->weak_from_this();
        auto bound = [wp, mf](const T& ev){
            if (auto sp = wp.lock()) if (auto* obj = dynamic_cast<C*>(sp.get())) (obj->*mf)(ev);
        };
        return scene_->events().OnceTracked<T>(entity_, std::move(bound), priority, wp);
    }

    // ---- EMIT HELPERS ----
    // Scene-local
    template <typename T> void Emit(const T& evt) { scene_->Emit<T>(evt); }
    template <typename T> void PostScene(T evt)   { scene_->Post<T>(std::move(evt)); }

    // Global
    template <typename T> void EmitGlobal(const T& evt) { ctx_->Emit<T>(evt); }
    template <typename T> void PostGlobal(T evt)        { ctx_->Post<T>(std::move(evt)); }

private:
    friend class Scripts; // component that attaches scripts and populates these
    Entity entity_{};
    EngineContext* ctx_{};
    Scene* scene_{};
};

} // namespace ptgn

namespace ptgn {

// Simple container component living on an Entity
class Scripts {
public:
    Scripts(EngineContext& ctx, Scene& sc) : ctx_(ctx), scene_(sc) {}

    template <typename T, typename... Args>
    T& Add(Entity owner, Args&&... args) {
        static_assert(std::is_base_of_v<ScriptBase, T>, "T must derive from ScriptBase");
        auto p = std::make_shared<T>(std::forward<Args>(args)...);
        p->entity_ = owner;
        p->ctx_    = &ctx_;
        p->scene_  = &scene_;
        list_.push_back(p);
        p->OnCreate();
        return *p;
    }

    template <typename T>
    void Remove() {
        auto it = std::remove_if(list_.begin(), list_.end(),
            [](const std::shared_ptr<ScriptBase>& s){ return dynamic_cast<T*>(s.get()) != nullptr; });
        list_.erase(it, list_.end()); // auto-unsub happens via weak tokens
    }

    void DestroyAll() { list_.clear(); }

private:
    EngineContext& ctx_;
    Scene& scene_;
    std::vector<std::shared_ptr<ScriptBase>> list_;
};

} // namespace ptgn

namespace ptgn {

struct EInventoryChanged {
    Entity who{};
    int    delta    = 0;
    int    newCount = 0;
};

struct EAnnounceGlobal { const char* text{}; };

} // namespace ptgn

namespace ptgn {

class PlayerInventoryUI : public ScriptBase {
public:
    void OnCreate() override {
        // Listen to scene-local inventory changes (player-specific UI)
        OnScene<EInventoryChanged>(&PlayerInventoryUI::OnInventoryChanged, /*prio=*/10);

        // Also listen to a global announcement (optional)
        OnGlobal<EAnnounceGlobal>(&PlayerInventoryUI::OnAnnounce);
    }

private:
    void OnInventoryChanged(const EInventoryChanged& e) {
        if (e.who != entity()) return;
        // update UI counter, play sfx, etc.
        printf("[UI] inventory now %d, delta: %d\n", e.newCount, e.delta);
    }
    void OnAnnounce(const EAnnounceGlobal& e) {
        (void)e; /* draw banner, etc. */
        std::cout << e.text << std::endl;
    }
};

} // namespace ptgn

namespace ptgn {

class LootPickup : public ScriptBase {
public:
    explicit LootPickup(int* playerCount) : counter_(playerCount) {}

    void OnCreate() override {
        // nothing to subscribe; will Emit when collected
    }

    void Collect(Entity player) {
        if (!counter_) return;
        *counter_ += 1;

        // Emit scene-local (other scene systems/scripts react)
        Emit(EInventoryChanged{ player, +1, *counter_ });

        // Emit a global announcement
        if (ctx()) ctx()->Emit(EAnnounceGlobal{ "picked up loot" });

        // Destroy this entity in your ECS (not shown)
    }

private:
    int* counter_{};
};

} // namespace ptgn

int main() {
    // Global event hub in the EngineContext
    ptgn::EventHub globalHub;
    ptgn::EngineContext ctx{ globalHub };

    // One scene with its own local hub
    ptgn::Scene scene(ctx);

    // Scripts component for a player entity within this scene
    Entity player{};
    ptgn::Scripts playerScripts(ctx, scene);
    playerScripts.Add<ptgn::PlayerInventoryUI>(player);

    // Loot entity + pickup script
    Entity loot{};
    ptgn::Scripts lootScripts(ctx, scene);
    int playerCount = 0;
    auto& pickup = lootScripts.Add<ptgn::LootPickup>(loot, &playerCount);

    // Simulate collection: this will Emit() scene-local and ctx->Emit() global
    pickup.Collect(player);
    pickup.Collect(player);
    pickup.Collect(player);

    // If anything used Post(), pump queues:
    ctx.PumpEvents();    // global
    scene.PumpEvents();  // scene

    // Optional compaction each frame end
    ctx.GCEvents();
    scene.GCEvents();

    // Destroy scripts (auto-unsubscribe via weak tokens)
    lootScripts.DestroyAll();
    playerScripts.DestroyAll();

    return 0;
}

/*Scene-local vs global*:

  * `Emit(evt)` inside a `ScriptBase` goes to the **scene’s** `EventHub`.
  * `ctx()->Emit(evt)` goes to the **global** `EventHub` in `EngineContext`.
* **Subscriptions**: call `OnScene<T>` / `OnGlobal<T>` (or `Once…`) inside `OnCreate()`. They automatically track the script’s lifetime with `weak_from_this()`, so no `OnDestroy`.
* **Iterator safety**: channels mark slots dead during publish; `GC()` compacts later.
* You can keep your input/physics/etc. systems publishing to **scene** or **global** hubs as appropriate.

If you want, I can also show a tiny macro layer to shorten `OnScene<Evt>(&Class::Method)` to `ON_SCENE(Evt, Method)`, but the above keeps it explicit and clear.
*/
// Subscribe to SCENE events (priority optional)
#define PTGN_ON_SCENE(EVT, METHOD) \
    this->OnScene<EVT>(&std::remove_cvref_t<decltype(*this)>::METHOD)

#define PTGN_ON_SCENE_P(EVT, METHOD, PRIO) \
    this->OnScene<EVT>(&std::remove_cvref_t<decltype(*this)>::METHOD, (PRIO))

// Subscribe once to SCENE events
#define PTGN_ONCE_SCENE(EVT, METHOD) \
    this->OnceScene<EVT>(&std::remove_cvref_t<decltype(*this)>::METHOD)

#define PTGN_ONCE_SCENE_P(EVT, METHOD, PRIO) \
    this->OnceScene<EVT>(&std::remove_cvref_t<decltype(*this)>::METHOD, (PRIO))

// Subscribe to GLOBAL events
#define PTGN_ON_GLOBAL(EVT, METHOD) \
    this->OnGlobal<EVT>(&std::remove_cvref_t<decltype(*this)>::METHOD)

#define PTGN_ON_GLOBAL_P(EVT, METHOD, PRIO) \
    this->OnGlobal<EVT>(&std::remove_cvref_t<decltype(*this)>::METHOD, (PRIO))

// Subscribe once to GLOBAL events
#define PTGN_ONCE_GLOBAL(EVT, METHOD) \
    this->OnceGlobal<EVT>(&std::remove_cvref_t<decltype(*this)>::METHOD)

#define PTGN_ONCE_GLOBAL_P(EVT, METHOD, PRIO) \
    this->OnceGlobal<EVT>(&std::remove_cvref_t<decltype(*this)>::METHOD, (PRIO))

// Emit helpers (sugar, just reads nicer in scripts)
#define PTGN_EMIT_SCENE(EVT_EXPR)   this->Emit((EVT_EXPR))
#define PTGN_POST_SCENE(EVT_EXPR)   this->PostScene((EVT_EXPR))
#define PTGN_EMIT_GLOBAL(EVT_EXPR)  this->EmitGlobal((EVT_EXPR))
#define PTGN_POST_GLOBAL(EVT_EXPR)  this->PostGlobal((EVT_EXPR))
// ---------------------------------------------------------------------------


// Button script example:

// ---------------------------------------------------------------------------
// Add this to your events section
namespace ptgn {

struct EButtonClick {
    Entity target{};       // which entity was clicked (the button entity)
    int    mouseButton=0;  // 0=left, 1=right, ... (optional)
    int    clicks=1;       // click count (single/double) (optional)
    // You can extend with modifiers, cursor pos, etc.
};

} // namespace ptgn
// ---------------------------------------------------------------------------


// ---------------------------------------------------------------------------
// Add this new base class next to your other scripts
namespace ptgn {

class BaseButtonScript : public ScriptBase {
public:
    void OnCreate() override {
        // Listen to scene-local button clicks; filter to our entity
        PTGN_ON_SCENE(EButtonClick, HandleButtonClick);
    }

    // Users override this:
    // Called when THIS entity is clicked.
    virtual void OnButtonClick(const EButtonClick& /*e*/) {}

    // Optional helpers
    void SetEnabled(bool on) { enabled_ = on; }
    bool Enabled() const     { return enabled_; }

private:
    void HandleButtonClick(const EButtonClick& e) {
        if (!enabled_) return;
        if (e.target != entity()) return;   // only react if we are the clicked button
        OnButtonClick(e);
    }

    bool enabled_ = true;
};

} // namespace ptgn
// ---------------------------------------------------------------------------


// ---------------------------------------------------------------------------
// Example: a concrete button script the user would write
namespace ptgn {

class RestartButton : public BaseButtonScript {
public:
    void OnButtonClick(const EButtonClick& e) override {
        (void)e;
        // Do your button action here
        PTGN_EMIT_GLOBAL(EAnnounceGlobal{ "restart requested" });
        std::cout << "[Button] Restart requested\n";
    }
};

} // namespace ptgn
// ---------------------------------------------------------------------------


// ---------------------------------------------------------------------------
// Example glue: wherever your UI/input system detects a click on an entity
// call this (scene-local emit so only scene scripts see it):

// Suppose you detected the left mouse button down on some UI button entity:
void EmitButtonClickFor(ptgn::Scene& scene, Entity buttonEntity) {
    scene.Emit(ptgn::EButtonClick{ buttonEntity, /*mouseButton*/0, /*clicks*/1 });
}