#include <imgui.h>
#include <imgui_stdlib.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace demo {

using EntityId = std::uint64_t;

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

struct Entity {
    EntityId id{ 0 };
    std::string name{ "Entity" };
    Transform transform{};
    bool visible{ true };
    Color tint{};

    bool operator==(const Entity&) const = default;
};

class Scene {
public:
    [[nodiscard]] Entity& CreateEntity(std::string name = "Entity") {
        Entity entity;
        entity.id = next_entity_id_++;
        entity.name = std::move(name);
        entities_.push_back(std::move(entity));
        return entities_.back();
    }

    [[nodiscard]] Entity* FindEntity(EntityId id) {
        auto it{ std::ranges::find(entities_, id, &Entity::id) };
        return it == entities_.end() ? nullptr : std::addressof(*it);
    }

    [[nodiscard]] const Entity* FindEntity(EntityId id) const {
        auto it{ std::ranges::find(entities_, id, &Entity::id) };
        return it == entities_.end() ? nullptr : std::addressof(*it);
    }

    [[nodiscard]] std::optional<std::size_t> FindEntityIndex(EntityId id) const {
        auto it{ std::ranges::find(entities_, id, &Entity::id) };
        if (it == entities_.end()) {
            return std::nullopt;
        }
        return static_cast<std::size_t>(std::distance(entities_.begin(), it));
    }

    bool RemoveEntity(EntityId id) {
        auto it{ std::ranges::find(entities_, id, &Entity::id) };
        if (it == entities_.end()) {
            return false;
        }
        entities_.erase(it);
        return true;
    }

    void InsertEntity(std::size_t index, Entity entity) {
        index = std::min(index, entities_.size());
        next_entity_id_ = std::max(next_entity_id_, entity.id + 1);
        entities_.insert(entities_.begin() + static_cast<std::ptrdiff_t>(index), std::move(entity));
    }

    [[nodiscard]] std::vector<Entity>& Entities() {
        return entities_;
    }

    [[nodiscard]] const std::vector<Entity>& Entities() const {
        return entities_;
    }

private:
    std::vector<Entity> entities_;
    EntityId next_entity_id_{ 1 };
};

} // namespace demo

namespace demo {

class EditorCommand {
public:
    virtual ~EditorCommand() = default;

    virtual void Undo(Scene& scene) = 0;
    virtual void Redo(Scene& scene) = 0;

    [[nodiscard]] virtual std::string_view Label() const = 0;
};

class EditEntityCommand final : public EditorCommand {
public:
    EditEntityCommand(std::string label, Entity before, Entity after)
        : label_{ std::move(label) }, before_{ std::move(before) }, after_{ std::move(after) } {}

    void Undo(Scene& scene) override {
        Apply(scene, before_);
    }

    void Redo(Scene& scene) override {
        Apply(scene, after_);
    }

    [[nodiscard]] std::string_view Label() const override {
        return label_;
    }

private:
    static void Apply(Scene& scene, const Entity& snapshot) {
        if (auto* entity{ scene.FindEntity(snapshot.id) }) {
            *entity = snapshot;
        }
    }

    std::string label_;
    Entity before_;
    Entity after_;
};

class CreateEntityCommand final : public EditorCommand {
public:
    CreateEntityCommand(Entity entity, std::size_t index)
        : entity_{ std::move(entity) }, index_{ index } {}

    void Undo(Scene& scene) override {
        scene.RemoveEntity(entity_.id);
    }

    void Redo(Scene& scene) override {
        if (!scene.FindEntity(entity_.id)) {
            scene.InsertEntity(index_, entity_);
        }
    }

    [[nodiscard]] std::string_view Label() const override {
        return "Create Entity";
    }

private:
    Entity entity_;
    std::size_t index_{ 0 };
};

class DeleteEntityCommand final : public EditorCommand {
public:
    DeleteEntityCommand(Entity entity, std::size_t index)
        : entity_{ std::move(entity) }, index_{ index } {}

    void Undo(Scene& scene) override {
        if (!scene.FindEntity(entity_.id)) {
            scene.InsertEntity(index_, entity_);
        }
    }

    void Redo(Scene& scene) override {
        scene.RemoveEntity(entity_.id);
    }

    [[nodiscard]] std::string_view Label() const override {
        return "Delete Entity";
    }

private:
    Entity entity_;
    std::size_t index_{ 0 };
};

class UndoStack {
public:
    struct HistoryEntry {
        std::string label;
        bool applied{ false };
    };

    template <typename Command, typename... Args>
    void Execute(Scene& scene, Args&&... args) {
        auto command{ std::make_unique<Command>(std::forward<Args>(args)...) };
        command->Redo(scene);
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

    void BeginEntityEdit(std::string label, const Entity& before) {
        if (active_edit_) {
            return;
        }

        active_edit_ = ActiveEdit{
            .label = std::move(label),
            .before = before,
        };
    }

    void CommitEntityEdit(const Entity& after) {
        if (!active_edit_ || active_edit_->before.id != after.id) {
            return;
        }

        ActiveEdit edit{ std::move(*active_edit_) };
        active_edit_.reset();

        if (edit.before == after) {
            return;
        }

        PushApplied(std::make_unique<EditEntityCommand>(
            std::move(edit.label),
            std::move(edit.before),
            after
        ));
    }

    void CommitImmediateEntityEdit(std::string label, const Entity& before, const Entity& after) {
        if (before == after) {
            return;
        }

        PushApplied(std::make_unique<EditEntityCommand>(
            std::move(label),
            before,
            after
        ));
    }

    void CancelEntityEdit(Scene& scene) {
        if (!active_edit_) {
            return;
        }

        if (auto* entity{ scene.FindEntity(active_edit_->before.id) }) {
            *entity = active_edit_->before;
        }

        active_edit_.reset();
    }

    [[nodiscard]] bool HasActiveEdit() const {
        return active_edit_.has_value();
    }

    // Structural editor operations must come after any in-progress inspector edit.
    // This preserves history ordering, e.g. Change Position -> Delete Entity.
    void CommitActiveEntityEdit(const Scene& scene) {
        if (!active_edit_) {
            return;
        }

        const Entity* entity{ scene.FindEntity(active_edit_->before.id) };
        if (!entity) {
            active_edit_.reset();
            return;
        }

        CommitEntityEdit(*entity);
    }

    [[nodiscard]] bool CanUndo() const {
        return cursor_ > 0;
    }

    [[nodiscard]] bool CanRedo() const {
        return cursor_ < commands_.size();
    }

    void Undo(Scene& scene) {
        if (active_edit_) {
            CancelEntityEdit(scene);
        }

        if (!CanUndo()) {
            return;
        }

        --cursor_;
        commands_[cursor_]->Undo(scene);
    }

    void Redo(Scene& scene) {
        if (active_edit_) {
            CancelEntityEdit(scene);
        }

        if (!CanRedo()) {
            return;
        }

        commands_[cursor_]->Redo(scene);
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
    struct ActiveEdit {
        std::string label;
        Entity before;
    };

    void DiscardRedoBranch() {
        if (cursor_ < commands_.size()) {
            commands_.erase(commands_.begin() + static_cast<std::ptrdiff_t>(cursor_), commands_.end());
        }
    }

    std::vector<std::unique_ptr<EditorCommand>> commands_;
    std::size_t cursor_{ 0 };
    std::optional<ActiveEdit> active_edit_;
};

} // namespace demo

namespace demo {
namespace {

struct EditorApplication {
    Scene scene;
    UndoStack undo;
    std::optional<EntityId> selected_entity;
};

void GlfwErrorCallback(int error, const char* description) {
    std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

void ValidateSelection(EditorApplication& app) {
    if (app.selected_entity && !app.scene.FindEntity(*app.selected_entity)) {
        app.selected_entity.reset();
    }
}

void TrackInspectorItem(
    UndoStack& undo,
    const Entity& frame_before,
    const Entity& current,
    std::string_view label,
    bool changed
) {
    const bool activated{ ImGui::IsItemActivated() };
    const bool committed{ ImGui::IsItemDeactivatedAfterEdit() };

    if (activated) {
        undo.BeginEntityEdit(std::string{ label }, frame_before);
    }

    if (committed) {
        undo.CommitEntityEdit(current);
        return;
    }

    // Buttons, menu selections, and some custom widgets may change without
    // remaining active across frames. Treat those as immediate edits.
    if (changed && !ImGui::IsItemActive() && !undo.HasActiveEdit()) {
        undo.CommitImmediateEntityEdit(std::string{ label }, frame_before, current);
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
            app.undo.Undo(app.scene);
        }
        ImGui::EndDisabled();

        ImGui::BeginDisabled(!app.undo.CanRedo());
        if (ImGui::MenuItem("Redo", "Ctrl/Cmd+Shift+Z")) {
            app.undo.Redo(app.scene);
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
    ImGui::SetNextWindowSize(ImVec2{ 500.0f, 65.0f }, ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    ImGui::BeginDisabled(!app.undo.CanUndo());
    if (ImGui::Button("Undo")) {
        app.undo.Undo(app.scene);
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!app.undo.CanRedo());
    if (ImGui::Button("Redo")) {
        app.undo.Redo(app.scene);
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::TextDisabled("Ctrl/Cmd+Z | Ctrl/Cmd+Shift+Z");

    ImGui::End();
}

void DrawSceneHierarchy(EditorApplication& app) {
    ImGui::SetNextWindowPos(ImVec2{ 10.0f, 105.0f }, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2{ 270.0f, 580.0f }, ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Scene Hierarchy")) {
        ImGui::End();
        return;
    }

    if (ImGui::Button("+ Entity")) {
        app.undo.CommitActiveEntityEdit(app.scene);

        Entity entity;
        entity.id = app.scene.CreateEntity().id;
        const auto index{ app.scene.FindEntityIndex(entity.id).value() };
        entity = *app.scene.FindEntity(entity.id);

        // Remove the direct creation, then let the command perform it so that
        // creation and future redo follow the same path.
        app.scene.RemoveEntity(entity.id);
        app.undo.Execute<CreateEntityCommand>(app.scene, entity, index);
        app.selected_entity = entity.id;
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(!app.selected_entity.has_value());
    if (ImGui::Button("Delete")) {
        app.undo.CommitActiveEntityEdit(app.scene);

        if (auto* entity{ app.scene.FindEntity(*app.selected_entity) }) {
            const auto index{ app.scene.FindEntityIndex(entity->id).value() };
            const Entity snapshot{ *entity };
            app.undo.Execute<DeleteEntityCommand>(app.scene, snapshot, index);
            app.selected_entity.reset();
        }
    }
    ImGui::EndDisabled();

    ImGui::Separator();

    for (const auto& entity : app.scene.Entities()) {
        ImGui::PushID(static_cast<int>(entity.id));
        const bool selected{ app.selected_entity == entity.id };

        std::string label{ entity.visible ? "[V] " : "[ ] " };
        label += entity.name;
        label += "##entity";

        if (ImGui::Selectable(label.c_str(), selected)) {
            app.selected_entity = entity.id;
        }

        ImGui::PopID();
    }

    ImGui::End();
}

void DrawInspector(EditorApplication& app) {
    ImGui::SetNextWindowPos(ImVec2{ 290.0f, 105.0f }, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2{ 440.0f, 580.0f }, ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Inspector")) {
        ImGui::End();
        return;
    }

    if (!app.selected_entity) {
        ImGui::TextDisabled("Select an entity.");
        ImGui::End();
        return;
    }

    Entity* entity{ app.scene.FindEntity(*app.selected_entity) };
    if (!entity) {
        ImGui::TextDisabled("The selected entity no longer exists.");
        ImGui::End();
        return;
    }

    ImGui::PushID(static_cast<int>(entity->id));

    {
        const Entity before{ *entity };
        const bool changed{ ImGui::InputText("Name", &entity->name) };
        TrackInspectorItem(app.undo, before, *entity, "Rename Entity", changed);
    }

    ImGui::TextDisabled("Entity ID: %llu", static_cast<unsigned long long>(entity->id));

    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();

        {
            const Entity before{ *entity };
            const bool changed{ DrawV2Control("Position", entity->transform.position, 0.25f) };
            TrackInspectorItem(app.undo, before, *entity, "Change Position", changed);
        }

        {
            const Entity before{ *entity };
            const bool changed{ ImGui::DragFloat(
                "Rotation",
                &entity->transform.rotation_degrees,
                0.5f,
                -360.0f,
                360.0f,
                "%.1f deg"
            ) };
            TrackInspectorItem(app.undo, before, *entity, "Change Rotation", changed);
        }

        {
            const Entity before{ *entity };
            const bool changed{ DrawV2Control("Scale", entity->transform.scale, 0.01f) };
            entity->transform.scale.x = std::max(entity->transform.scale.x, 0.01f);
            entity->transform.scale.y = std::max(entity->transform.scale.y, 0.01f);
            TrackInspectorItem(app.undo, before, *entity, "Change Scale", changed);
        }

        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();

        {
            const Entity before{ *entity };
            const bool changed{ ImGui::Checkbox("Visible", &entity->visible) };
            TrackInspectorItem(app.undo, before, *entity, "Toggle Visibility", changed);
        }

        {
            const Entity before{ *entity };
            std::array color{
                entity->tint.r,
                entity->tint.g,
                entity->tint.b,
                entity->tint.a,
            };

            const bool changed{ ImGui::ColorEdit4("Tint", color.data()) };
            if (changed) {
                entity->tint = Color{ color[0], color[1], color[2], color[3] };
            }
            TrackInspectorItem(app.undo, before, *entity, "Change Tint", changed);
        }

        ImGui::Unindent();
    }

    ImGui::Separator();
    ImGui::TextWrapped(
        "Drag edits preview live but produce one history entry when the widget is released. "
        "Checkboxes and other discrete controls commit immediately."
    );

    ImGui::PopID();
    ImGui::End();
}

void DrawHistory(EditorApplication& app) {
    ImGui::SetNextWindowPos(ImVec2{ 740.0f, 105.0f }, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2{ 340.0f, 580.0f }, ImGuiCond_FirstUseEver);

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

    // Let active text fields keep their own text-level undo behavior.
    if (!command_modifier || io.WantTextInput) {
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        if (io.KeyShift) {
            app.undo.Redo(app.scene);
        } else {
            app.undo.Undo(app.scene);
        }
    } else if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
        app.undo.Redo(app.scene);
    }
}

void SeedScene(EditorApplication& app) {
    auto& camera{ app.scene.CreateEntity("Camera") };
    camera.transform.position = V2{ 0.0f, 0.0f };

    auto& player{ app.scene.CreateEntity("Player") };
    player.transform.position = V2{ 120.0f, 80.0f };
    player.tint = Color{ 0.2f, 0.7f, 1.0f, 1.0f };

    auto& light{ app.scene.CreateEntity("Light") };
    light.transform.position = V2{ -150.0f, 60.0f };
    light.tint = Color{ 1.0f, 0.8f, 0.25f, 1.0f };

    app.selected_entity = player.id;
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
        1100,
        720,
        "Protegon Inspector Undo Demo",
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
    SeedScene(app);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ValidateSelection(app);
        HandleShortcuts(app);
        DrawMainMenu(app);
        DrawToolbar(app);
        DrawSceneHierarchy(app);
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
