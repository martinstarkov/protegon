// sandbox demo

#include <imgui.h>
#include <imgui_stdlib.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>

#include <ecs/ecs.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace demo {

using EntityId = std::uint64_t;
using SceneId = std::uint64_t;
using Entity = ecs::Entity;
using Manager = ecs::Manager;

struct UUID {
    EntityId value{ 0 };

    bool operator==(const UUID&) const = default;
};

struct Name {
    std::string value{ "Entity" };

    bool operator==(const Name&) const = default;
};

struct V2 {
    float x{ 0.0f };
    float y{ 0.0f };

    bool operator==(const V2&) const = default;
};

struct Color {
    float r{ 1.0f };
    float g{ 1.0f };
    float b{ 1.0f };
    float a{ 1.0f };

    bool operator==(const Color&) const = default;
};

struct Transform {
    V2 position{};
    float rotation_degrees{ 0.0f };
    V2 scale{ 1.0f, 1.0f };

    bool operator==(const Transform&) const = default;
};

struct Visible {
    bool value{ true };

    bool operator==(const Visible&) const = default;
};

struct Tint {
    Color value{};

    bool operator==(const Tint&) const = default;
};

class ComponentSnapshot {
public:
    virtual ~ComponentSnapshot() = default;

    virtual void Restore(Entity entity) const = 0;
};

template <typename TComponent>
class TypedComponentSnapshot final : public ComponentSnapshot {
public:
    explicit TypedComponentSnapshot(const TComponent& value) : value_{ value } {}

    void Restore(Entity entity) const override {
        entity.Add<TComponent>(value_);
    }

private:
    TComponent value_;
};

class ComponentSnapshotRegistry {
public:
    template <typename TComponent>
    void Register() {
        static_assert(
            std::copy_constructible<TComponent>,
            "Undo snapshots require copy constructible editor components"
        );

        capture_functions_.push_back([](const Entity& entity) -> std::unique_ptr<ComponentSnapshot> {
            const auto* component{ entity.TryGet<TComponent>() };
            if (!component) {
                return nullptr;
            }
            return std::make_unique<TypedComponentSnapshot<TComponent>>(*component);
        });
    }

    [[nodiscard]] std::vector<std::unique_ptr<ComponentSnapshot>> Capture(
        const Entity& entity
    ) const {
        std::vector<std::unique_ptr<ComponentSnapshot>> result;
        result.reserve(capture_functions_.size());

        for (const auto capture : capture_functions_) {
            if (auto component{ capture(entity) }) {
                result.push_back(std::move(component));
            }
        }

        return result;
    }

private:
    using CaptureFunction = std::unique_ptr<ComponentSnapshot> (*)(const Entity&);

    std::vector<CaptureFunction> capture_functions_;
};

struct EntitySnapshot {
    EntityId id{ 0 };
    std::vector<std::unique_ptr<ComponentSnapshot>> components;

    EntitySnapshot() = default;
    EntitySnapshot(EntitySnapshot&&) noexcept = default;
    EntitySnapshot& operator=(EntitySnapshot&&) noexcept = default;

    EntitySnapshot(const EntitySnapshot&) = delete;
    EntitySnapshot& operator=(const EntitySnapshot&) = delete;
};

class Scene {
public:
    Scene() {
        snapshot_registry_.Register<Name>();
        snapshot_registry_.Register<Transform>();
        snapshot_registry_.Register<Visible>();
        snapshot_registry_.Register<Tint>();
    }

    [[nodiscard]] Entity CreateEntity(std::string name = "Entity") {
        const EntityId id{ next_entity_id_++ };
        Entity entity{ manager_.CreateEntity() };

        entity.Add<UUID>(UUID{ id });
        entity.Add<Name>(Name{ std::move(name) });
        entity.Add<Transform>(Transform{});
        entity.Add<Visible>(Visible{});
        entity.Add<Tint>(Tint{});

        manager_.Refresh();
        entities_.insert_or_assign(id, entity);
        entity_order_.push_back(id);
        return entity;
    }

    [[nodiscard]] std::optional<Entity> FindEntity(EntityId id) {
        const auto it{ entities_.find(id) };
        if (it == entities_.end() || !it->second) {
            return std::nullopt;
        }
        return it->second;
    }

    [[nodiscard]] std::optional<Entity> FindEntity(EntityId id) const {
        const auto it{ entities_.find(id) };
        if (it == entities_.end() || !it->second) {
            return std::nullopt;
        }
        return it->second;
    }

    [[nodiscard]] std::optional<std::size_t> FindEntityIndex(EntityId id) const {
        const auto it{ std::ranges::find(entity_order_, id) };
        if (it == entity_order_.end()) {
            return std::nullopt;
        }
        return static_cast<std::size_t>(std::distance(entity_order_.begin(), it));
    }

    [[nodiscard]] std::optional<EntitySnapshot> CaptureEntity(EntityId id) const {
        const auto entity{ FindEntity(id) };
        if (!entity) {
            return std::nullopt;
        }

        EntitySnapshot snapshot;
        snapshot.id = id;
        snapshot.components = snapshot_registry_.Capture(*entity);
        return snapshot;
    }

    [[nodiscard]] Entity RestoreEntity(const EntitySnapshot& snapshot, std::size_t index) {
        if (const auto existing{ FindEntity(snapshot.id) }) {
            return *existing;
        }

        Entity entity{ manager_.CreateEntity() };
        entity.Add<UUID>(UUID{ snapshot.id });

        for (const auto& component : snapshot.components) {
            component->Restore(entity);
        }

        manager_.Refresh();
        entities_.insert_or_assign(snapshot.id, entity);

        index = std::min(index, entity_order_.size());
        entity_order_.insert(
            entity_order_.begin() + static_cast<std::ptrdiff_t>(index),
            snapshot.id
        );

        next_entity_id_ = std::max(next_entity_id_, snapshot.id + 1);
        return entity;
    }

    bool DestroyEntity(EntityId id) {
        auto entity{ FindEntity(id) };
        if (!entity) {
            return false;
        }

        entity->Destroy();
        manager_.Refresh();
        entities_.erase(id);
        std::erase(entity_order_, id);
        return true;
    }

    [[nodiscard]] const std::vector<EntityId>& EntityOrder() const {
        return entity_order_;
    }

private:
    Manager manager_;
    ComponentSnapshotRegistry snapshot_registry_;
    std::unordered_map<EntityId, Entity> entities_;
    std::vector<EntityId> entity_order_;
    EntityId next_entity_id_{ 1 };
};

struct SceneDocument {
    SceneId id{ 0 };
    std::string key;
    Scene scene;
};

struct EditorSelection {
    std::optional<SceneId> scene;
    std::optional<EntityId> entity;

    bool operator==(const EditorSelection&) const = default;
};

class EditorModel {
public:
    [[nodiscard]] SceneDocument& CreateScene(std::string key) {
        auto document{ std::make_unique<SceneDocument>() };
        document->id = next_scene_id_++;
        document->key = std::move(key);

        SceneDocument& result{ *document };
        scenes_.push_back(std::move(document));

        if (!selection_.scene) {
            selection_.scene = result.id;
        }

        return result;
    }

    [[nodiscard]] SceneDocument* FindScene(SceneId id) {
        const auto it{ std::ranges::find_if(scenes_, [id](const auto& scene) {
            return scene->id == id;
        }) };
        return it == scenes_.end() ? nullptr : it->get();
    }

    [[nodiscard]] const SceneDocument* FindScene(SceneId id) const {
        const auto it{ std::ranges::find_if(scenes_, [id](const auto& scene) {
            return scene->id == id;
        }) };
        return it == scenes_.end() ? nullptr : it->get();
    }

    [[nodiscard]] SceneDocument* SelectedScene() {
        return selection_.scene ? FindScene(*selection_.scene) : nullptr;
    }

    [[nodiscard]] const SceneDocument* SelectedScene() const {
        return selection_.scene ? FindScene(*selection_.scene) : nullptr;
    }

    [[nodiscard]] std::optional<Entity> FindEntity(SceneId scene_id, EntityId entity_id) {
        SceneDocument* document{ FindScene(scene_id) };
        return document ? document->scene.FindEntity(entity_id) : std::nullopt;
    }

    [[nodiscard]] std::optional<Entity> FindEntity(SceneId scene_id, EntityId entity_id) const {
        const SceneDocument* document{ FindScene(scene_id) };
        return document ? document->scene.FindEntity(entity_id) : std::nullopt;
    }

    [[nodiscard]] const std::vector<std::unique_ptr<SceneDocument>>& Scenes() const {
        return scenes_;
    }

    [[nodiscard]] EditorSelection Selection() const {
        return selection_;
    }

    void SetSelection(EditorSelection selection) {
        if (!selection.scene || !FindScene(*selection.scene)) {
            selection_ = {};
            return;
        }

        if (selection.entity && !FindEntity(*selection.scene, *selection.entity)) {
            selection.entity.reset();
        }

        selection_ = selection;
    }

    void ValidateSelection() {
        SetSelection(selection_);
    }

private:
    std::vector<std::unique_ptr<SceneDocument>> scenes_;
    EditorSelection selection_;
    SceneId next_scene_id_{ 1 };
};

struct EntityReference {
    SceneId scene{ 0 };
    EntityId entity{ 0 };

    bool operator==(const EntityReference&) const = default;
};

} // namespace demo

namespace demo {

class EditorCommand {
public:
    virtual ~EditorCommand() = default;

    virtual void Undo(EditorModel& model) = 0;
    virtual void Redo(EditorModel& model) = 0;

    [[nodiscard]] virtual std::string_view Label() const = 0;
};

template <typename TComponent>
class EditComponentCommand final : public EditorCommand {
public:
    EditComponentCommand(
        std::string label,
        SceneId scene_id,
        EntityId entity_id,
        TComponent before,
        TComponent after
    ) :
        label_{ std::move(label) },
        scene_id_{ scene_id },
        entity_id_{ entity_id },
        before_{ std::move(before) },
        after_{ std::move(after) } {}

    void Undo(EditorModel& model) override {
        Apply(model, before_);
    }

    void Redo(EditorModel& model) override {
        Apply(model, after_);
    }

    [[nodiscard]] std::string_view Label() const override {
        return label_;
    }

private:
    void Apply(EditorModel& model, const TComponent& value) const {
        if (auto entity{ model.FindEntity(scene_id_, entity_id_) }) {
            entity->template Add<TComponent>(value);
        }
    }

    std::string label_;
    SceneId scene_id_{ 0 };
    EntityId entity_id_{ 0 };
    TComponent before_;
    TComponent after_;
};

class SelectionCommand final : public EditorCommand {
public:
    SelectionCommand(std::string label, EditorSelection before, EditorSelection after) :
        label_{ std::move(label) }, before_{ before }, after_{ after } {}

    void Undo(EditorModel& model) override {
        model.SetSelection(before_);
    }

    void Redo(EditorModel& model) override {
        model.SetSelection(after_);
    }

    [[nodiscard]] std::string_view Label() const override {
        return label_;
    }

private:
    std::string label_;
    EditorSelection before_;
    EditorSelection after_;
};

class RenameSceneKeyCommand final : public EditorCommand {
public:
    RenameSceneKeyCommand(SceneId scene_id, std::string before, std::string after) :
        scene_id_{ scene_id }, before_{ std::move(before) }, after_{ std::move(after) } {}

    void Undo(EditorModel& model) override {
        Apply(model, before_);
    }

    void Redo(EditorModel& model) override {
        Apply(model, after_);
    }

    [[nodiscard]] std::string_view Label() const override {
        return "Rename Scene Key";
    }

private:
    void Apply(EditorModel& model, const std::string& key) const {
        if (SceneDocument* scene{ model.FindScene(scene_id_) }) {
            scene->key = key;
        }
    }

    SceneId scene_id_{ 0 };
    std::string before_;
    std::string after_;
};

class CreateEntityCommand final : public EditorCommand {
public:
    CreateEntityCommand(
        SceneId scene_id,
        EntitySnapshot snapshot,
        std::size_t index,
        EditorSelection before_selection,
        EditorSelection after_selection
    ) :
        scene_id_{ scene_id },
        snapshot_{ std::move(snapshot) },
        index_{ index },
        before_selection_{ before_selection },
        after_selection_{ after_selection } {}

    void Undo(EditorModel& model) override {
        if (SceneDocument* scene{ model.FindScene(scene_id_) }) {
            scene->scene.DestroyEntity(snapshot_.id);
        }
        model.SetSelection(before_selection_);
    }

    void Redo(EditorModel& model) override {
        if (SceneDocument* scene{ model.FindScene(scene_id_) }) {
            (void)scene->scene.RestoreEntity(snapshot_, index_);
        }
        model.SetSelection(after_selection_);
    }

    [[nodiscard]] std::string_view Label() const override {
        return "Create Entity";
    }

private:
    SceneId scene_id_{ 0 };
    EntitySnapshot snapshot_;
    std::size_t index_{ 0 };
    EditorSelection before_selection_;
    EditorSelection after_selection_;
};

class DeleteEntityCommand final : public EditorCommand {
public:
    DeleteEntityCommand(
        SceneId scene_id,
        EntitySnapshot snapshot,
        std::size_t index,
        EditorSelection before_selection,
        EditorSelection after_selection
    ) :
        scene_id_{ scene_id },
        snapshot_{ std::move(snapshot) },
        index_{ index },
        before_selection_{ before_selection },
        after_selection_{ after_selection } {}

    void Undo(EditorModel& model) override {
        if (SceneDocument* scene{ model.FindScene(scene_id_) }) {
            (void)scene->scene.RestoreEntity(snapshot_, index_);
        }
        model.SetSelection(before_selection_);
    }

    void Redo(EditorModel& model) override {
        if (SceneDocument* scene{ model.FindScene(scene_id_) }) {
            scene->scene.DestroyEntity(snapshot_.id);
        }
        model.SetSelection(after_selection_);
    }

    [[nodiscard]] std::string_view Label() const override {
        return "Delete Entity";
    }

private:
    SceneId scene_id_{ 0 };
    EntitySnapshot snapshot_;
    std::size_t index_{ 0 };
    EditorSelection before_selection_;
    EditorSelection after_selection_;
};

class ActiveEdit {
public:
    virtual ~ActiveEdit() = default;

    [[nodiscard]] virtual bool MatchesComponent(
        SceneId,
        EntityId,
        std::type_index
    ) const {
        return false;
    }

    [[nodiscard]] virtual bool MatchesSceneKey(SceneId) const {
        return false;
    }

    virtual void Cancel(EditorModel& model) const = 0;
    [[nodiscard]] virtual std::unique_ptr<EditorCommand> Commit(EditorModel& model) const = 0;
};

template <typename TComponent>
class ActiveComponentEdit final : public ActiveEdit {
public:
    ActiveComponentEdit(
        std::string label,
        SceneId scene_id,
        EntityId entity_id,
        TComponent before
    ) :
        label_{ std::move(label) },
        scene_id_{ scene_id },
        entity_id_{ entity_id },
        before_{ std::move(before) } {}

    [[nodiscard]] bool MatchesComponent(
        SceneId scene_id,
        EntityId entity_id,
        std::type_index component_type
    ) const override {
        return scene_id_ == scene_id && entity_id_ == entity_id &&
               component_type == std::type_index{ typeid(TComponent) };
    }

    void Cancel(EditorModel& model) const override {
        if (auto entity{ model.FindEntity(scene_id_, entity_id_) }) {
            entity->template Add<TComponent>(before_);
        }
    }

    [[nodiscard]] std::unique_ptr<EditorCommand> Commit(EditorModel& model) const override {
        const auto entity{ model.FindEntity(scene_id_, entity_id_) };
        if (!entity) {
            return nullptr;
        }

        const auto* after{ entity->template TryGet<TComponent>() };
        if (!after || before_ == *after) {
            return nullptr;
        }

        return std::make_unique<EditComponentCommand<TComponent>>(
            label_,
            scene_id_,
            entity_id_,
            before_,
            *after
        );
    }

private:
    std::string label_;
    SceneId scene_id_{ 0 };
    EntityId entity_id_{ 0 };
    TComponent before_;
};

class ActiveSceneKeyEdit final : public ActiveEdit {
public:
    ActiveSceneKeyEdit(SceneId scene_id, std::string before) :
        scene_id_{ scene_id }, before_{ std::move(before) } {}

    [[nodiscard]] bool MatchesSceneKey(SceneId scene_id) const override {
        return scene_id_ == scene_id;
    }

    void Cancel(EditorModel& model) const override {
        if (SceneDocument* scene{ model.FindScene(scene_id_) }) {
            scene->key = before_;
        }
    }

    [[nodiscard]] std::unique_ptr<EditorCommand> Commit(EditorModel& model) const override {
        const SceneDocument* scene{ model.FindScene(scene_id_) };
        if (!scene || scene->key == before_) {
            return nullptr;
        }

        return std::make_unique<RenameSceneKeyCommand>(scene_id_, before_, scene->key);
    }

private:
    SceneId scene_id_{ 0 };
    std::string before_;
};

class UndoStack {
public:
    struct HistoryEntry {
        std::string label;
        bool applied{ false };
    };

    template <typename Command, typename... Args>
    void Execute(EditorModel& model, Args&&... args) {
        auto command{ std::make_unique<Command>(std::forward<Args>(args)...) };
        command->Redo(model);
        PushApplied(std::move(command));
    }

    void PushApplied(std::unique_ptr<EditorCommand> command) {
        if (!command) {
            return;
        }

        DiscardRedoBranch();
        commands_.push_back(std::move(command));
        cursor_ = commands_.size();
    }

    template <typename TComponent>
    void BeginComponentEdit(
        std::string label,
        SceneId scene_id,
        EntityId entity_id,
        const TComponent& before
    ) {
        if (active_edit_) {
            return;
        }

        active_edit_ = std::make_unique<ActiveComponentEdit<TComponent>>(
            std::move(label),
            scene_id,
            entity_id,
            before
        );
    }

    template <typename TComponent>
    void CommitComponentEdit(EditorModel& model, SceneId scene_id, EntityId entity_id) {
        if (!active_edit_ ||
            !active_edit_->MatchesComponent(
                scene_id,
                entity_id,
                std::type_index{ typeid(TComponent) }
            )) {
            return;
        }

        CommitActiveEdit(model);
    }

    template <typename TComponent>
    void CommitImmediateComponentEdit(
        std::string label,
        SceneId scene_id,
        EntityId entity_id,
        const TComponent& before,
        const TComponent& after
    ) {
        if (before == after) {
            return;
        }

        PushApplied(std::make_unique<EditComponentCommand<TComponent>>(
            std::move(label),
            scene_id,
            entity_id,
            before,
            after
        ));
    }

    void BeginSceneKeyEdit(SceneId scene_id, const std::string& before) {
        if (active_edit_) {
            return;
        }

        active_edit_ = std::make_unique<ActiveSceneKeyEdit>(scene_id, before);
    }

    void CommitSceneKeyEdit(EditorModel& model, SceneId scene_id) {
        if (!active_edit_ || !active_edit_->MatchesSceneKey(scene_id)) {
            return;
        }

        CommitActiveEdit(model);
    }

    void CommitImmediateSceneKeyEdit(
        SceneId scene_id,
        const std::string& before,
        const std::string& after
    ) {
        if (before == after) {
            return;
        }

        PushApplied(std::make_unique<RenameSceneKeyCommand>(scene_id, before, after));
    }

    void CancelActiveEdit(EditorModel& model) {
        if (!active_edit_) {
            return;
        }

        active_edit_->Cancel(model);
        active_edit_.reset();
    }

    void CommitActiveEdit(EditorModel& model) {
        if (!active_edit_) {
            return;
        }

        auto command{ active_edit_->Commit(model) };
        active_edit_.reset();
        PushApplied(std::move(command));
    }

    [[nodiscard]] bool HasActiveEdit() const {
        return active_edit_ != nullptr;
    }

    [[nodiscard]] bool CanUndo() const {
        return cursor_ > 0;
    }

    [[nodiscard]] bool CanRedo() const {
        return cursor_ < commands_.size();
    }

    void Undo(EditorModel& model) {
        if (active_edit_) {
            CancelActiveEdit(model);
        }

        if (!CanUndo()) {
            return;
        }

        --cursor_;
        commands_[cursor_]->Undo(model);
    }

    void Redo(EditorModel& model) {
        if (active_edit_) {
            CancelActiveEdit(model);
        }

        if (!CanRedo()) {
            return;
        }

        commands_[cursor_]->Redo(model);
        ++cursor_;
    }

    void Clear() {
        commands_.clear();
        cursor_ = 0;
        active_edit_.reset();
    }

    [[nodiscard]] std::vector<HistoryEntry> History() const {
        std::vector<HistoryEntry> result;
        result.reserve(commands_.size());

        for (std::size_t i{ 0 }; i < commands_.size(); ++i) {
            result.push_back(HistoryEntry{
                .label = std::string{ commands_[i]->Label() },
                .applied = i < cursor_,
            });
        }

        return result;
    }

    [[nodiscard]] std::size_t Cursor() const {
        return cursor_;
    }

private:
    void DiscardRedoBranch() {
        if (cursor_ < commands_.size()) {
            commands_.erase(
                commands_.begin() + static_cast<std::ptrdiff_t>(cursor_),
                commands_.end()
            );
        }
    }

    std::vector<std::unique_ptr<EditorCommand>> commands_;
    std::size_t cursor_{ 0 };
    std::unique_ptr<ActiveEdit> active_edit_;
};

} // namespace demo

namespace demo {
namespace {

struct EditorApplication {
    EditorModel model;
    UndoStack undo;
    std::optional<EntityReference> position_pick_entity;
};

void GlfwErrorCallback(int error, const char* description) {
    std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

void ValidateEditorState(EditorApplication& app) {
    app.model.ValidateSelection();

    if (app.position_pick_entity &&
        !app.model.FindEntity(
            app.position_pick_entity->scene,
            app.position_pick_entity->entity
        )) {
        app.position_pick_entity.reset();
    }
}

void Undo(EditorApplication& app) {
    app.position_pick_entity.reset();
    app.undo.Undo(app.model);
}

void Redo(EditorApplication& app) {
    app.position_pick_entity.reset();
    app.undo.Redo(app.model);
}

void SetSelection(
    EditorApplication& app,
    EditorSelection selection,
    std::string label
) {
    app.undo.CommitActiveEdit(app.model);

    const EditorSelection before{ app.model.Selection() };
    if (before == selection) {
        return;
    }

    app.position_pick_entity.reset();
    app.undo.Execute<SelectionCommand>(
        app.model,
        std::move(label),
        before,
        selection
    );
}

template <typename TComponent>
void TrackInspectorItem(
    UndoStack& undo,
    EditorModel& model,
    SceneId scene_id,
    EntityId entity_id,
    const TComponent& frame_before,
    std::string_view label,
    bool changed
) {
    const bool activated{ ImGui::IsItemActivated() };
    const bool committed{ ImGui::IsItemDeactivatedAfterEdit() };

    if (activated) {
        undo.BeginComponentEdit<TComponent>(
            std::string{ label },
            scene_id,
            entity_id,
            frame_before
        );
    }

    if (committed) {
        undo.CommitComponentEdit<TComponent>(model, scene_id, entity_id);
        return;
    }

    if (changed && !ImGui::IsItemActive() && !undo.HasActiveEdit()) {
        const auto entity{ model.FindEntity(scene_id, entity_id) };
        if (!entity) {
            return;
        }

        const auto* current{ entity->TryGet<TComponent>() };
        if (!current) {
            return;
        }

        undo.CommitImmediateComponentEdit<TComponent>(
            std::string{ label },
            scene_id,
            entity_id,
            frame_before,
            *current
        );
    }
}

void TrackSceneKeyItem(
    UndoStack& undo,
    EditorModel& model,
    SceneId scene_id,
    const std::string& frame_before,
    bool changed
) {
    const bool activated{ ImGui::IsItemActivated() };
    const bool committed{ ImGui::IsItemDeactivatedAfterEdit() };

    if (activated) {
        undo.BeginSceneKeyEdit(scene_id, frame_before);
    }

    if (committed) {
        undo.CommitSceneKeyEdit(model, scene_id);
        return;
    }

    if (changed && !ImGui::IsItemActive() && !undo.HasActiveEdit()) {
        const SceneDocument* scene{ model.FindScene(scene_id) };
        if (scene) {
            undo.CommitImmediateSceneKeyEdit(scene_id, frame_before, scene->key);
        }
    }
}

bool DrawV2Control(const char* label, V2& value, float speed = 0.1f) {
    std::array values{ value.x, value.y };
    const bool changed{ ImGui::DragFloat2(label, values.data(), speed) };

    if (changed) {
        value.x = values[0];
        value.y = values[1];
    }

    return changed;
}

void DrawMainMenu(EditorApplication& app) {
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }

    if (ImGui::BeginMenu("Edit")) {
        ImGui::BeginDisabled(!app.undo.CanUndo());
        if (ImGui::MenuItem("Undo", "Ctrl/Cmd+Z")) {
            Undo(app);
        }
        ImGui::EndDisabled();

        ImGui::BeginDisabled(!app.undo.CanRedo());
        if (ImGui::MenuItem("Redo", "Ctrl/Cmd+Shift+Z")) {
            Redo(app);
        }
        ImGui::EndDisabled();

        ImGui::Separator();
        if (ImGui::MenuItem("Clear History")) {
            app.undo.Clear();
        }

        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

void DrawToolbar(EditorApplication& app) {
    ImGui::SetNextWindowPos(ImVec2{ 10.0f, 30.0f }, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2{ 1260.0f, 65.0f }, ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    ImGui::BeginDisabled(!app.undo.CanUndo());
    if (ImGui::Button("Undo")) {
        Undo(app);
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!app.undo.CanRedo());
    if (ImGui::Button("Redo")) {
        Redo(app);
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::TextDisabled("Ctrl/Cmd+Z | Ctrl/Cmd+Shift+Z");

    ImGui::End();
}

void DrawSceneList(EditorApplication& app) {
    ImGui::SetNextWindowPos(ImVec2{ 10.0f, 105.0f }, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2{ 240.0f, 220.0f }, ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Scene List")) {
        ImGui::End();
        return;
    }

    const EditorSelection selection{ app.model.Selection() };

    for (const auto& scene : app.model.Scenes()) {
        ImGui::PushID(static_cast<int>(scene->id));
        const bool selected{ selection.scene == scene->id };

        if (ImGui::Selectable(scene->key.c_str(), selected)) {
            SetSelection(
                app,
                EditorSelection{ .scene = scene->id, .entity = std::nullopt },
                "Select Scene"
            );
        }

        ImGui::PopID();
    }

    ImGui::Separator();

    if (SceneDocument* scene{ app.model.SelectedScene() }) {
        const std::string before{ scene->key };
        const bool changed{ ImGui::InputText("Scene Key", &scene->key) };
        TrackSceneKeyItem(app.undo, app.model, scene->id, before, changed);

        ImGui::TextDisabled("Stable scene ID: %llu", static_cast<unsigned long long>(scene->id));
    } else {
        ImGui::TextDisabled("Select a scene.");
    }

    ImGui::End();
}

void DrawSceneHierarchy(EditorApplication& app) {
    ImGui::SetNextWindowPos(ImVec2{ 10.0f, 335.0f }, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2{ 240.0f, 445.0f }, ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Scene Hierarchy")) {
        ImGui::End();
        return;
    }

    SceneDocument* document{ app.model.SelectedScene() };
    if (!document) {
        ImGui::TextDisabled("Select a scene.");
        ImGui::End();
        return;
    }

    const SceneId scene_id{ document->id };
    const EditorSelection selection{ app.model.Selection() };

    if (ImGui::Button("+ Entity")) {
        app.undo.CommitActiveEdit(app.model);

        const EditorSelection before_selection{ app.model.Selection() };
        const Entity entity{ document->scene.CreateEntity() };
        const EntityId id{ entity.Get<UUID>().value };
        const std::size_t index{ document->scene.FindEntityIndex(id).value() };
        auto snapshot{ document->scene.CaptureEntity(id) };

        if (snapshot) {
            document->scene.DestroyEntity(id);
            app.undo.Execute<CreateEntityCommand>(
                app.model,
                scene_id,
                std::move(*snapshot),
                index,
                before_selection,
                EditorSelection{ .scene = scene_id, .entity = id }
            );
        }
    }

    ImGui::SameLine();
    const bool selected_entity_in_scene{
        selection.scene == scene_id && selection.entity.has_value()
    };
    ImGui::BeginDisabled(!selected_entity_in_scene);
    if (ImGui::Button("Delete")) {
        app.undo.CommitActiveEdit(app.model);

        const EditorSelection before_selection{ app.model.Selection() };
        const EntityId id{ *before_selection.entity };
        const auto index{ document->scene.FindEntityIndex(id) };
        auto snapshot{ document->scene.CaptureEntity(id) };

        if (index && snapshot) {
            app.position_pick_entity.reset();
            app.undo.Execute<DeleteEntityCommand>(
                app.model,
                scene_id,
                std::move(*snapshot),
                *index,
                before_selection,
                EditorSelection{ .scene = scene_id, .entity = std::nullopt }
            );
        }
    }
    ImGui::EndDisabled();

    ImGui::Separator();

    for (const EntityId id : document->scene.EntityOrder()) {
        const auto entity{ document->scene.FindEntity(id) };
        if (!entity) {
            continue;
        }

        ImGui::PushID(static_cast<int>(id));
        const bool selected{ selection.scene == scene_id && selection.entity == id };

        const auto& name{ entity->Get<Name>() };
        const auto& visible{ entity->Get<Visible>() };

        std::string label{ visible.value ? "[V] " : "[ ] " };
        label += name.value;
        label += "##entity";

        if (ImGui::Selectable(label.c_str(), selected)) {
            SetSelection(
                app,
                EditorSelection{ .scene = scene_id, .entity = id },
                "Select Entity"
            );
        }

        ImGui::PopID();
    }

    ImGui::End();
}

void DrawSceneView(EditorApplication& app) {
    ImGui::SetNextWindowPos(ImVec2{ 260.0f, 105.0f }, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2{ 600.0f, 675.0f }, ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Scene View")) {
        ImGui::End();
        return;
    }

    SceneDocument* document{ app.model.SelectedScene() };
    if (!document) {
        ImGui::TextDisabled("Select a scene.");
        ImGui::End();
        return;
    }

    const SceneId scene_id{ document->id };

    if (app.position_pick_entity) {
        ImGui::TextUnformatted("Position picking: click the viewport to set X and Y.");
        ImGui::SameLine();
        if (ImGui::SmallButton("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            app.position_pick_entity.reset();
        }
    } else {
        ImGui::TextDisabled("Select an entity, then use Position > Pick in the Inspector.");
    }

    ImVec2 canvas_position{ ImGui::GetCursorScreenPos() };
    ImVec2 canvas_size{ ImGui::GetContentRegionAvail() };
    canvas_size.x = std::max(canvas_size.x, 50.0f);
    canvas_size.y = std::max(canvas_size.y, 50.0f);

    ImGui::InvisibleButton(
        "##SceneCanvas",
        canvas_size,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight
    );

    const bool hovered{ ImGui::IsItemHovered() };
    const bool left_clicked{ hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) };
    const bool right_clicked{ hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right) };
    ImDrawList* draw_list{ ImGui::GetWindowDrawList() };
    const ImVec2 canvas_end{
        canvas_position.x + canvas_size.x,
        canvas_position.y + canvas_size.y
    };
    const ImVec2 origin{
        canvas_position.x + canvas_size.x * 0.5f,
        canvas_position.y + canvas_size.y * 0.5f
    };

    const ImU32 background_color{ ImGui::GetColorU32(ImGuiCol_ChildBg) };
    const ImU32 border_color{ ImGui::GetColorU32(ImGuiCol_Border) };
    const ImU32 grid_color{ ImGui::GetColorU32(ImVec4{ 0.35f, 0.38f, 0.43f, 0.25f }) };
    const ImU32 axis_color{ ImGui::GetColorU32(ImVec4{ 0.65f, 0.68f, 0.74f, 0.65f }) };
    const ImU32 selected_color{ ImGui::GetColorU32(ImGuiCol_Text) };

    draw_list->AddRectFilled(canvas_position, canvas_end, background_color);
    draw_list->AddRect(canvas_position, canvas_end, border_color);

    constexpr float grid_spacing{ 50.0f };
    for (float x{ origin.x }; x < canvas_end.x; x += grid_spacing) {
        draw_list->AddLine(ImVec2{ x, canvas_position.y }, ImVec2{ x, canvas_end.y }, grid_color);
    }
    for (float x{ origin.x - grid_spacing }; x > canvas_position.x; x -= grid_spacing) {
        draw_list->AddLine(ImVec2{ x, canvas_position.y }, ImVec2{ x, canvas_end.y }, grid_color);
    }
    for (float y{ origin.y }; y < canvas_end.y; y += grid_spacing) {
        draw_list->AddLine(ImVec2{ canvas_position.x, y }, ImVec2{ canvas_end.x, y }, grid_color);
    }
    for (float y{ origin.y - grid_spacing }; y > canvas_position.y; y -= grid_spacing) {
        draw_list->AddLine(ImVec2{ canvas_position.x, y }, ImVec2{ canvas_end.x, y }, grid_color);
    }

    draw_list->AddLine(
        ImVec2{ canvas_position.x, origin.y },
        ImVec2{ canvas_end.x, origin.y },
        axis_color,
        1.5f
    );
    draw_list->AddLine(
        ImVec2{ origin.x, canvas_position.y },
        ImVec2{ origin.x, canvas_end.y },
        axis_color,
        1.5f
    );

    const auto world_to_screen = [origin](V2 world) {
        return ImVec2{ origin.x + world.x, origin.y - world.y };
    };
    const auto screen_to_world = [origin](ImVec2 screen) {
        return V2{ screen.x - origin.x, origin.y - screen.y };
    };

    const ImVec2 mouse_position{ ImGui::GetIO().MousePos };
    const V2 mouse_world{ screen_to_world(mouse_position) };

    const bool picking_in_scene{
        app.position_pick_entity && app.position_pick_entity->scene == scene_id
    };

    if (picking_in_scene && hovered) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        draw_list->AddLine(
            ImVec2{ mouse_position.x - 8.0f, mouse_position.y },
            ImVec2{ mouse_position.x + 8.0f, mouse_position.y },
            selected_color,
            2.0f
        );
        draw_list->AddLine(
            ImVec2{ mouse_position.x, mouse_position.y - 8.0f },
            ImVec2{ mouse_position.x, mouse_position.y + 8.0f },
            selected_color,
            2.0f
        );

        char coordinates[64];
        std::snprintf(
            coordinates,
            sizeof(coordinates),
            "X %.0f, Y %.0f",
            mouse_world.x,
            mouse_world.y
        );
        draw_list->AddText(
            ImVec2{ mouse_position.x + 12.0f, mouse_position.y + 12.0f },
            selected_color,
            coordinates
        );
    }

    if (right_clicked && picking_in_scene) {
        app.position_pick_entity.reset();
    } else if (left_clicked && picking_in_scene) {
        const EntityId entity_id{ app.position_pick_entity->entity };
        if (auto entity{ document->scene.FindEntity(entity_id) }) {
            const Transform before{ entity->Get<Transform>() };
            Transform after{ before };
            after.position = mouse_world;
            entity->Add<Transform>(after);
            app.undo.CommitImmediateComponentEdit<Transform>(
                "Pick Position",
                scene_id,
                entity_id,
                before,
                after
            );
        }
        app.position_pick_entity.reset();
    } else if (left_clicked && !app.position_pick_entity) {
        constexpr float selection_radius{ 14.0f };
        std::optional<EntityId> closest_entity;
        float closest_distance_squared{ selection_radius * selection_radius };

        for (const EntityId id : document->scene.EntityOrder()) {
            const auto entity{ document->scene.FindEntity(id) };
            if (!entity || !entity->Get<Visible>().value) {
                continue;
            }

            const ImVec2 entity_screen{ world_to_screen(entity->Get<Transform>().position) };
            const float dx{ mouse_position.x - entity_screen.x };
            const float dy{ mouse_position.y - entity_screen.y };
            const float distance_squared{ dx * dx + dy * dy };
            if (distance_squared <= closest_distance_squared) {
                closest_distance_squared = distance_squared;
                closest_entity = id;
            }
        }

        SetSelection(
            app,
            EditorSelection{ .scene = scene_id, .entity = closest_entity },
            closest_entity ? "Select Entity" : "Clear Entity Selection"
        );
    }

    const EditorSelection selection{ app.model.Selection() };

    for (const EntityId id : document->scene.EntityOrder()) {
        const auto entity{ document->scene.FindEntity(id) };
        if (!entity || !entity->Get<Visible>().value) {
            continue;
        }

        const Transform& transform{ entity->Get<Transform>() };
        const Tint& tint{ entity->Get<Tint>() };
        const Name& name{ entity->Get<Name>() };
        const ImVec2 entity_position{ world_to_screen(transform.position) };

        if (entity_position.x < canvas_position.x || entity_position.x > canvas_end.x ||
            entity_position.y < canvas_position.y || entity_position.y > canvas_end.y) {
            continue;
        }

        const ImU32 entity_color{ ImGui::GetColorU32(ImVec4{
            tint.value.r,
            tint.value.g,
            tint.value.b,
            tint.value.a
        }) };
        draw_list->AddCircleFilled(entity_position, 8.0f, entity_color);

        if (selection.scene == scene_id && selection.entity == id) {
            draw_list->AddCircle(entity_position, 12.0f, selected_color, 0, 2.0f);
        }

        draw_list->AddText(
            ImVec2{ entity_position.x + 12.0f, entity_position.y - 7.0f },
            selected_color,
            name.value.c_str()
        );
    }

    ImGui::End();
}

void DrawInspector(EditorApplication& app) {
    ImGui::SetNextWindowPos(ImVec2{ 870.0f, 105.0f }, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2{ 400.0f, 430.0f }, ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Inspector")) {
        ImGui::End();
        return;
    }

    const EditorSelection selection{ app.model.Selection() };
    if (!selection.scene || !selection.entity) {
        ImGui::TextDisabled("Select an entity.");
        ImGui::End();
        return;
    }

    const SceneId scene_id{ *selection.scene };
    const EntityId entity_id{ *selection.entity };
    const auto entity_handle{ app.model.FindEntity(scene_id, entity_id) };
    if (!entity_handle) {
        ImGui::TextDisabled("The selected entity no longer exists.");
        ImGui::End();
        return;
    }

    Entity entity{ *entity_handle };
    ImGui::PushID(static_cast<int>(entity_id));

    {
        auto& name{ entity.Get<Name>() };
        const Name before{ name };
        const bool changed{ ImGui::InputText("Name", &name.value) };
        TrackInspectorItem(
            app.undo,
            app.model,
            scene_id,
            entity_id,
            before,
            "Rename Entity",
            changed
        );
    }

    ImGui::TextDisabled("Entity UUID: %llu", static_cast<unsigned long long>(entity_id));
    ImGui::TextDisabled("Current ECS ID: %u", entity.GetId());

    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();

        {
            auto& transform{ entity.Get<Transform>() };
            const Transform before{ transform };

            ImGui::PushID("Position");
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Position");
            ImGui::SameLine(92.0f);

            constexpr float pick_button_width{ 58.0f };
            const float value_width{
                std::max(80.0f, ImGui::GetContentRegionAvail().x - pick_button_width - 4.0f)
            };
            ImGui::SetNextItemWidth(value_width);
            const bool changed{ DrawV2Control("##XY", transform.position, 0.25f) };
            TrackInspectorItem(
                app.undo,
                app.model,
                scene_id,
                entity_id,
                before,
                "Change Position",
                changed
            );

            ImGui::SameLine();
            const EntityReference reference{ .scene = scene_id, .entity = entity_id };
            const bool picking{ app.position_pick_entity == reference };
            if (ImGui::Button(picking ? "Cancel" : "Pick", ImVec2{ pick_button_width, 0.0f })) {
                app.undo.CommitActiveEdit(app.model);
                if (picking) {
                    app.position_pick_entity.reset();
                } else {
                    app.position_pick_entity = reference;
                }
            }
            ImGui::PopID();

            if (picking) {
                ImGui::TextDisabled("Click in Scene View to set both X and Y.");
            }
        }

        {
            auto& transform{ entity.Get<Transform>() };
            const Transform before{ transform };
            const bool changed{ ImGui::DragFloat(
                "Rotation",
                &transform.rotation_degrees,
                0.5f,
                -360.0f,
                360.0f,
                "%.1f deg"
            ) };
            TrackInspectorItem(
                app.undo,
                app.model,
                scene_id,
                entity_id,
                before,
                "Change Rotation",
                changed
            );
        }

        {
            auto& transform{ entity.Get<Transform>() };
            const Transform before{ transform };
            const bool changed{ DrawV2Control("Scale", transform.scale, 0.01f) };
            transform.scale.x = std::max(transform.scale.x, 0.01f);
            transform.scale.y = std::max(transform.scale.y, 0.01f);
            TrackInspectorItem(
                app.undo,
                app.model,
                scene_id,
                entity_id,
                before,
                "Change Scale",
                changed
            );
        }

        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();

        {
            auto& visible{ entity.Get<Visible>() };
            const Visible before{ visible };
            const bool changed{ ImGui::Checkbox("Visible", &visible.value) };
            TrackInspectorItem(
                app.undo,
                app.model,
                scene_id,
                entity_id,
                before,
                "Toggle Visibility",
                changed
            );
        }

        {
            auto& tint{ entity.Get<Tint>() };
            const Tint before{ tint };
            std::array color{
                tint.value.r,
                tint.value.g,
                tint.value.b,
                tint.value.a,
            };

            const bool changed{ ImGui::ColorEdit4("Tint", color.data()) };
            if (changed) {
                tint.value = Color{ color[0], color[1], color[2], color[3] };
            }
            TrackInspectorItem(
                app.undo,
                app.model,
                scene_id,
                entity_id,
                before,
                "Change Tint",
                changed
            );
        }

        ImGui::Unindent();
    }

    ImGui::Separator();
    ImGui::TextWrapped(
        "Entity and scene selection are editor commands. Scene-key commands target stable scene "
        "IDs, while component commands target stable scene IDs and entity UUIDs."
    );

    ImGui::PopID();
    ImGui::End();
}

void DrawHistory(EditorApplication& app) {
    ImGui::SetNextWindowPos(ImVec2{ 870.0f, 545.0f }, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2{ 400.0f, 235.0f }, ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Command History")) {
        ImGui::End();
        return;
    }

    const auto history{ app.undo.History() };
    const auto cursor{ app.undo.Cursor() };

    ImGui::Text("Applied commands: %zu / %zu", cursor, history.size());

    if (app.undo.HasActiveEdit()) {
        ImGui::SameLine();
        ImGui::TextDisabled("(editing...)");
    }

    ImGui::Separator();

    if (history.empty()) {
        ImGui::TextDisabled("No commands executed yet.");
    }

    for (std::size_t i{ 0 }; i < history.size(); ++i) {
        const auto& entry{ history[i] };
        ImGui::PushID(static_cast<int>(i));

        if (i == cursor) {
            ImGui::SeparatorText("Redo branch");
        }

        if (!entry.applied) {
            ImGui::BeginDisabled();
        }

        ImGui::BulletText(
            "%zu. %s%s",
            i + 1,
            entry.label.c_str(),
            entry.applied ? "" : " (undone)"
        );

        if (!entry.applied) {
            ImGui::EndDisabled();
        }

        ImGui::PopID();
    }

    if (cursor == history.size() && !history.empty()) {
        ImGui::SeparatorText("Current state");
    }

    ImGui::End();
}

void HandleShortcuts(EditorApplication& app) {
    ImGuiIO& io{ ImGui::GetIO() };
    const bool command_modifier{ io.KeyCtrl || io.KeySuper };

    if (!command_modifier || io.WantTextInput) {
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        if (io.KeyShift) {
            Redo(app);
        } else {
            Undo(app);
        }
    } else if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
        Redo(app);
    }
}

void SeedScenes(EditorApplication& app) {
    SceneDocument& main_scene{ app.model.CreateScene("Main") };

    Entity camera{ main_scene.scene.CreateEntity("Camera") };
    camera.Get<Transform>().position = V2{ 0.0f, 0.0f };

    Entity player{ main_scene.scene.CreateEntity("Player") };
    player.Get<Transform>().position = V2{ 120.0f, 80.0f };
    player.Get<Tint>().value = Color{ 0.2f, 0.7f, 1.0f, 1.0f };

    Entity light{ main_scene.scene.CreateEntity("Light") };
    light.Get<Transform>().position = V2{ -150.0f, 60.0f };
    light.Get<Tint>().value = Color{ 1.0f, 0.8f, 0.25f, 1.0f };

    SceneDocument& menu_scene{ app.model.CreateScene("Menu") };

    Entity title{ menu_scene.scene.CreateEntity("Title") };
    title.Get<Transform>().position = V2{ 0.0f, 120.0f };
    title.Get<Tint>().value = Color{ 0.8f, 0.5f, 1.0f, 1.0f };

    Entity play_button{ menu_scene.scene.CreateEntity("Play Button") };
    play_button.Get<Transform>().position = V2{ 0.0f, -20.0f };
    play_button.Get<Tint>().value = Color{ 0.35f, 0.9f, 0.45f, 1.0f };

    SceneDocument& credits_scene{ app.model.CreateScene("Credits") };

    Entity credits{ credits_scene.scene.CreateEntity("Credits Text") };
    credits.Get<Transform>().position = V2{ 0.0f, 40.0f };
    credits.Get<Tint>().value = Color{ 1.0f, 0.75f, 0.3f, 1.0f };

    app.model.SetSelection(EditorSelection{
        .scene = main_scene.id,
        .entity = player.Get<UUID>().value
    });
}

} // namespace
} // namespace demo

int main() {
    using namespace demo;

    glfwSetErrorCallback(GlfwErrorCallback);
    if (!glfwInit()) {
        return 1;
    }

#if defined(__APPLE__)
    const char* glsl_version{ "#version 150" };
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#else
    const char* glsl_version{ "#version 130" };
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    GLFWwindow* window{ glfwCreateWindow(
        1280,
        800,
        "Protegon Undoable Editor State Demo",
        nullptr,
        nullptr
    ) };

    if (!window) {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io{ ImGui::GetIO() };
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
#if defined(__APPLE__)
    io.ConfigMacOSXBehaviors = true;
#endif

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    EditorApplication app;
    SeedScenes(app);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ValidateEditorState(app);
        HandleShortcuts(app);
        DrawMainMenu(app);
        DrawToolbar(app);
        DrawSceneList(app);
        DrawSceneHierarchy(app);
        DrawSceneView(app);
        DrawInspector(app);
        DrawHistory(app);

        ImGui::Render();

        int framebuffer_width{ 0 };
        int framebuffer_height{ 0 };
        glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
        glViewport(0, 0, framebuffer_width, framebuffer_height);
        glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
