Awesome—let’s bolt a simple, plugin-friendly **system registry/scheduler** onto the scene system you already have. You’ll get:

* Phases: `Startup`, `PreUpdate`, `Update`, `PostUpdate`, `PreRender`, `Render`, `PostRender`, `Shutdown`
* Easy registration (plugins or local lambdas)
* Per-scene pipeline compilation (each scene picks what runs)
* Toggleable built-in engine systems (physics, controllers, etc.)
* Deterministic ordering (priority + “after” dependencies)
* No globals required

Below is a compact pattern you can paste in and shape to taste.

---

# 1) Phases & descriptors

```cpp
// Phases.h
enum class Phase : uint8_t {
  Startup, PreUpdate, Update, PostUpdate, PreRender, Render, PostRender, Shutdown
};

class Registry;       // your ECS/world
struct EngineContext; // renderer, input, assets, time, ...

using SystemFn = void(*)(Registry&, EngineContext&);

struct SystemDesc {
  const char* name;              // unique-ish name for logs & deps
  Phase       phase;
  int         priority = 0;      // higher runs later (stable-sorted)
  std::vector<std::string> after;// run after these names (within same phase)
  bool        runOnce = false;   // for Startup/Shutdown (or any "one-shot")
  SystemFn    fn;
  // Optional categorization:
  std::vector<std::string> tags; // e.g. {"physics","core"} for filtering
};
```

---

# 2) Registrar (what plugins call)

```cpp
// SystemRegistrar.h
class SystemRegistrar {
public:
  void add(SystemDesc d) { all_.push_back(std::move(d)); }
  const std::vector<SystemDesc>& all() const { return all_; }
private:
  std::vector<SystemDesc> all_;
};

// Convenience macro for static plugins (optional)
#define REGISTER_SYSTEM(R, NAME, PHASE, PRIORITY, RUNONCE, FN, ...) \
  (R).add(SystemDesc{ NAME, PHASE, PRIORITY, /*after*/{}, RUNONCE, FN, /*tags*/{__VA_ARGS__} })
```

---

# 3) Scheduler (per-scene pipeline)

Each **Scene** gets a small scheduler that compiles from the global registrar, filtered by the scene’s choices (tags/feature flags). No globals: you pass references in the scene.

```cpp
// SystemScheduler.h
struct CompileFilter {
  // Include systems whose tags intersect this set (empty => include all).
  std::vector<std::string> includeTags;
  // Exclude systems with any of these tags.
  std::vector<std::string> excludeTags;
  // Optional explicit allowlist/denylist by name.
  std::vector<std::string> allowNames, denyNames;
};

class SystemScheduler {
public:
  SystemScheduler(Registry& reg, EngineContext& ctx) : reg_(reg), ctx_(ctx) {}

  void compile(const SystemRegistrar& reg, const CompileFilter& flt) {
    // filter
    std::vector<SystemDesc> pool;
    for (auto& d : reg.all()) if (accept(d, flt)) pool.push_back(d);

    // bucket by phase
    for (auto& v : phases_) v.clear();
    for (auto& d : pool) phases_[(int)d.phase].push_back(d);

    // sort by priority then topo sort by "after"
    for (auto& v : phases_) {
      std::stable_sort(v.begin(), v.end(),
          [](auto& a, auto& b){ return a.priority < b.priority; });
      topoOrder(v); // see below
    }
    runOnce_.clear();
  }

  void run(Phase ph) {
    auto& v = phases_[(int)ph];
    for (auto& d : v) {
      if (d.runOnce) {
        if (!runOnce_.insert(hash(d)).second) continue; // already ran
      }
      d.fn(reg_, ctx_);
    }
  }

private:
  Registry& reg_;
  EngineContext& ctx_;
  // one vector per phase
  std::array<std::vector<SystemDesc>, 7> phases_;
  std::unordered_set<uint64_t> runOnce_;

  static bool hasAny(const std::vector<std::string>& a, const std::vector<std::string>& b){
    if (a.empty() || b.empty()) return false;
    for (auto& x : a) for (auto& y : b) if (x==y) return true; return false;
  }
  static bool in(const std::string& n, const std::vector<std::string>& vs){
    return std::find(vs.begin(), vs.end(), n)!=vs.end();
  }
  static bool accept(const SystemDesc& d, const CompileFilter& f){
    if (!f.includeTags.empty() && !hasAny(d.tags, f.includeTags)) return false;
    if (hasAny(d.tags, f.excludeTags)) return false;
    if (!f.allowNames.empty() && !in(d.name, f.allowNames)) return false;
    if (!f.denyNames.empty()  &&  in(d.name, f.denyNames))  return false;
    return true;
  }
  static uint64_t hash(const SystemDesc& d){
    uint64_t h=1469598103934665603ull; for (auto* c=d.name; *c; ++c){ h^=(uint8_t)*c; h*=1099511628211ull; } return h ^ (uint64_t)d.phase;
  }
  static void topoOrder(std::vector<SystemDesc>& v){
    std::unordered_map<std::string,int> idx; for (int i=0;i<(int)v.size();++i) idx[v[i].name]=i;
    std::vector<int> indeg(v.size(),0); std::vector<std::vector<int>> adj(v.size());
    for (int i=0;i<(int)v.size();++i) for (auto& dep : v[i].after){
      auto it=idx.find(dep); if(it==idx.end()) continue; adj[it->second].push_back(i); indeg[i]++; }
    std::queue<int> q; for (int i=0;i<(int)v.size();++i) if(!indeg[i]) q.push(i);
    std::vector<SystemDesc> out; out.reserve(v.size());
    while(!q.empty()){ int i=q.front(); q.pop(); out.push_back(v[i]); for(int j:adj[i]) if(--indeg[j]==0) q.push(j); }
    if(out.size()==v.size()) v.swap(out); /* else cycle => keep current stable order (warn if you want) */
  }
};
```

---

# 4) Scene integration

Add a scheduler to your existing `Scene` and call it from `update()/draw()` using the granular phases.

```cpp
// Scene.h (augment your existing base)
class Scene {
public:
  virtual ~Scene() = default;

  // New hooks the scene can override to pick systems:
  virtual CompileFilter systemFilter() const { return {}; } // default: all
  virtual void registerLocalSystems(SystemRegistrar& r) {}   // scene-local systems

  // Lifecycle remains the same:
  virtual void onEnter() {}
  virtual void onExit()  {}
  virtual void update()  {}  // scene's own imperative logic (optional)
  virtual void draw()    {}  // scene's own rendering (optional)

  // Engine will provide these at construction/bind time:
  void _attachSchedulers(SystemScheduler* sched) { sched_ = sched; }

protected:
  SystemScheduler* sched_ = nullptr; // not owning; set by SceneManager
};
```

SceneManager wires it:

```cpp
// SceneManager.cpp (key bits)
void SceneManager::createScene(std::unique_ptr<Scene> s) {
  // Build per-scene registrar: start from global, allow scene to add local
  SystemRegistrar local = globalRegistrar_;   // copy from engine
  s->registerLocalSystems(local);

  // Per-scene scheduler compiled with the scene's filter
  s->sched_ = new SystemScheduler(s->registry_, ctx_);
  s->sched_->compile(local, s->systemFilter());
  s->onEnter();

  // store s; make sure to delete scheduler on scene destruction
}

void SceneManager::tickActiveScenes() {
  for (auto& layer : layers_) {
    auto* sc = layer.ptr.get();
    // Systems around the scene's imperative update:
    sc->sched_->run(Phase::PreUpdate);
    sc->sched_->run(Phase::Update);
    sc->sched_->run(Phase::PostUpdate);
  }
}
void SceneManager::drawActiveScenes() {
  for (auto& layer : layers_) {
    auto* sc = layer.ptr.get();
    sc->sched_->run(Phase::PreRender);
    sc->sched_->run(Phase::Render);
    sc->sched_->run(Phase::PostRender);
  }
}
```

> If you use the “overlays & transitions” manager we discussed earlier, apply your overlay policies to decide which scenes’ schedulers get called per phase.

---

# 5) Selecting built-in engine systems (physics, controllers, etc.)

Treat engine systems as **tagged** entries in the **global** registrar; scenes opt in via `CompileFilter`:

```cpp
// Engine boots and fills global registrar once:
SystemRegistrar global;
REGISTER_SYSTEM(global, "PhysicsStep", Phase::Update,   50, false,
  +[](Registry& r, EngineContext& c){ /* step physics */ },
  "physics","core");

REGISTER_SYSTEM(global, "CharacterController", Phase::Update, 60, false,
  +[](Registry& r, EngineContext& c){ /* move players */ },
  "controllers","gameplay");

REGISTER_SYSTEM(global, "SpriteRender", Phase::Render, 100, false,
  +[](Registry& r, EngineContext& c){ /* draw sprites */ },
  "render","core");
```

A scene that wants **physics + controllers + render**:

```cpp
class GameplayScene : public Scene {
public:
  CompileFilter systemFilter() const override {
    return CompileFilter{
      .includeTags = {"physics","controllers","render"} // pick subsets
    };
  }
};
```

A **menu scene** that only wants UI render:

```cpp
class MenuScene : public Scene {
public:
  CompileFilter systemFilter() const override {
    return CompileFilter{
      .includeTags = {"render","ui"},
      .excludeTags = {"physics","controllers"}
    };
  }
};
```

You can also **add local systems** only for this scene:

```cpp
void GameplayScene::registerLocalSystems(SystemRegistrar& r) {
  r.add(SystemDesc{
    .name="WavesSpawner",
    .phase=Phase::PreUpdate,
    .priority=10,
    .after={"PhysicsStep"}, // if needed
    .runOnce=false,
    .fn=+[](Registry& reg, EngineContext& ctx){ /* ... */ },
    .tags={"gameplay"}
  });
}
```

---

# 6) Example frame

```cpp
// per frame
for (auto& scene : activeScenesInOrder) {
  scene->sched_->run(Phase::Startup);    // (no-op after first frame due to runOnce)
  scene->sched_->run(Phase::PreUpdate);
  scene->update();                       // optional imperative scene code
  scene->sched_->run(Phase::Update);
  scene->sched_->run(Phase::PostUpdate);
}

for (auto& scene : activeScenesInOrder) {
  scene->sched_->run(Phase::PreRender);
  scene->draw();                         // optional imperative draw
  scene->sched_->run(Phase::Render);
  scene->sched_->run(Phase::PostRender);
}
```

For **transitions/overlays**, just skip `Update` for scenes below a modal (policy), but still call their `Render` if your transition wants both layers visible.

---

# 7) Quality-of-life (optional)

* **Per-phase enable toggles**: allow a scene to return booleans like `wantsUpdatePhase()` / `wantsRenderPhase()` to skip compilation for certain phases.
* **Named pipelines**: cache the compiled vectors so switching scenes is O(1).
* **Metrics hooks**: time each system call; log slow ones with name/phase.
* **Testability**: you can unit-test a scene’s pipeline by creating a `SystemRegistrar` with a few test systems and asserting the run order.

---

## TL;DR

* Keep your existing Scene/SceneManager.
* Give each scene a **per-scene scheduler** compiled from a **global registrar** plus **scene local systems**.
* Systems declare **phase**, **priority**, **dependencies**, **tags**, and **runOnce**.
* Scenes select **which built-ins** they want via `includeTags/excludeTags/allowNames`.
* The SceneManager calls the scheduler’s phases around the scene’s own `update()/draw()`, respecting overlay/transition policies.

If you share which ECS you’re using (EnTT/Flecs/custom), I can swap the `Registry` calls with concrete examples and wire in per-scene resources (e.g., `registry.ctx()` in EnTT).
