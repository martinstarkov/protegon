// ============================== INPUT ROUTER ================================
namespace ptgn {

// Very small router that: listens to global raw mouse events
// → hit-tests into the currently active scenes
// → emits semantic scene-local pointer events.
class InputRouter : public ScriptBase {
public:
    // You can swap the active scene list at runtime if you want
    explicit InputRouter(std::vector<Scene*> activeScenes)
        : activeScenes_(std::move(activeScenes)) {}

    void OnCreate() override {
        OnGlobal<EMouseMove, InputRouter>(&InputRouter::OnMouseMove);
        OnGlobal<EMouseBtn,  InputRouter>(&InputRouter::OnMouseBtn);
    }

    void SetActiveScenes(std::vector<Scene*> scenes) { activeScenes_ = std::move(scenes); }

private:
    std::vector<Scene*> activeScenes_{};
    Entity hover_{};
    Entity capture_{};
    int clickCount_ = 0;

    // ---------- Demo hit test ----------
    // This demo uses a super simple hit-test:
    //  * x < 100 → scene 0; target is scene0Button
    //  * x >=100 → scene 1; target is scene1Button
    // In a real engine, replace this with your actual UI layer.
    Entity scene0Button_{ reinterpret_cast<void*>(0xA) };
    Entity scene1Button_{ reinterpret_cast<void*>(0xB) };

    std::pair<Scene*, Entity> HitTestTopmost(float x, float /*y*/) {
        if (activeScenes_.empty()) return {nullptr, {}};
        if (activeScenes_.size() == 1) return { activeScenes_[0], scene0Button_ };
        if (x < 100.0f)   return { activeScenes_[0], scene0Button_ };
        else              return { activeScenes_[1], scene1Button_ };
    }

    Scene* SceneFor(Entity e) {
        if (e == scene0Button_) return activeScenes_.empty() ? nullptr : activeScenes_[0];
        if (activeScenes_.size() > 1 && e == scene1Button_) return activeScenes_[1];
        return nullptr;
    }

    // ---------- Routing ----------
    void OnMouseMove(const EMouseMove& e) {
        auto [scene, target] = capture_ ? std::pair{ SceneFor(capture_), capture_ }
                                        : HitTestTopmost(e.x, e.y);
        if (!scene) return;

        if (!capture_) {
            if (target != hover_) {
                if (hover_) scene->Emit(EPointerOut{hover_});
                hover_ = target;
                if (hover_) scene->Emit(EPointerOver{hover_, e.x, e.y});
            }
        } else {
            // could emit drag here if desired
        }
    }

    void OnMouseBtn(const EMouseBtn& e) {
        auto [scene, target] = capture_ ? std::pair{ SceneFor(capture_), capture_ }
                                        : HitTestTopmost(e.x, e.y);
        if (!scene) return;

        if (e.down) {
            capture_ = target;
            scene->Emit(EPointerDown{target, e.button, e.x, e.y});
        } else {
            scene->Emit(EPointerUp{target, e.button, e.x, e.y});
            if (target == capture_) {
                scene->Emit(EPointerClick{target, e.button, ++clickCount_, e.x, e.y});
            }
            capture_ = {};
        }
    }
};

} // namespace ptgn



// ============================== DEMO MAIN ===================================
int main() {
    using namespace ptgn;

    // Global event hub in the EngineContext
    ptgn::EventHub globalHub;
    ptgn::EngineContext ctx{ globalHub };

    // Two active scenes
    ptgn::Scene sceneA(ctx);
    ptgn::Scene sceneB(ctx);
    std::vector<ptgn::Scene*> activeScenes{ &sceneA, &sceneB };

    // Scripts containers
    Entity any{};
    ptgn::Scripts sysScripts(ctx, sceneA); // attach systems anywhere; they listen globally
    ptgn::Scripts aScripts(ctx, sceneA);
    ptgn::Scripts bScripts(ctx, sceneB);

    // Attach the input router (knows about both scenes)
    sysScripts.Add<ptgn::InputRouter>(any, activeScenes);

    // Attach a button in each scene (entities are placeholders)
    Entity btnA{ reinterpret_cast<void*>(0xA) };
    Entity btnB{ reinterpret_cast<void*>(0xB) };
    aScripts.Add<ptgn::SceneButtonA>(btnA);
    bScripts.Add<ptgn::SceneButtonB>(btnB);

    // Optional: other demo scripts
    Entity player{};
    int playerCount = 0;
    aScripts.Add<ptgn::PlayerInventoryUI>(player);
    auto& loot = aScripts.Add<ptgn::LootPickup>(any, &playerCount);

    // --------------------------- Update Loop (demo) --------------------------
    auto PumpAll = [&]{
        // 1) deliver global events (router listens here)
        ctx.PumpEvents();

        // 2) router re-emits scene-local; now pump scenes so scripts get them
        sceneA.PumpEvents();
        sceneB.PumpEvents();

        // 3) (optional) GC
        ctx.GCEvents();
        sceneA.GCEvents();
        sceneB.GCEvents();
    };

    // Frame 1: click in Scene A (x = 10)
    ctx.Emit(EMouseMove{10, 50, 0, 0});
    ctx.Emit(EMouseBtn{0, true, 10, 50});   // down
    ctx.Emit(EMouseBtn{0, false, 10, 50});  // up → click
    PumpAll();

    // Frame 2: click in Scene B (x = 150)
    ctx.Emit(EMouseMove{150, 50, 0, 0});
    ctx.Emit(EMouseBtn{0, true, 150, 50});  // down
    ctx.Emit(EMouseBtn{0, false, 150, 50}); // up → click
    PumpAll();

    // Frame 3: show that other systems still work and are scene-local/global
    loot.Collect(player);
    PumpAll();

    // destroy
    bScripts.DestroyAll();
    aScripts.DestroyAll();
    sysScripts.DestroyAll();

    return 0;
}