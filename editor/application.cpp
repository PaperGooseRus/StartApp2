#include "application.hpp"
#include "imgui_internal.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
namespace studio {
namespace {
ImTextureID texture(SDL_Texture* t) { return static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(t)); }
ImU32 rgba(pixel::Color c, float opacity = 1) { return IM_COL32(c.r, c.g, c.b, static_cast<int>(c.a * opacity)); }
const char* layers[] = {"Фон", "Объекты", "Персонажи", "Передний план"};
void textField(const char* label, std::string& value, bool& change, std::size_t size = 256) {
    char buffer[513]{}; std::snprintf(buffer, std::min(size + 1, sizeof(buffer)), "%s", value.c_str());
    if (ImGui::InputText(label, buffer, std::min(size + 1, sizeof(buffer)))) { value = buffer; change = true; }
}
}
Application::Application(SDL_Window* window, SDL_Renderer* renderer) : window_(window), renderer_(renderer), view_(renderer) {
    target_.reset(SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 768, 576));
    if (!target_) throw std::runtime_error("Renderer does not support editor viewport textures");
    SDL_SetTextureBlendMode(target_.get(), SDL_BLENDMODE_NONE);
    std::unique_ptr<char, decltype(&SDL_free)> pref(SDL_GetPrefPath("PaperGoose", "PixelStudio"), SDL_free);
    projectPath_ = (pref ? std::filesystem::u8path(pref.get()) : std::filesystem::current_path()) / "Моя игра.pxproj";
    selection_ = doc_.project.game.gate;
    if (std::filesystem::is_regular_file(projectPath_)) {
        try { open(projectPath_); }
        catch (const std::exception& e) {
            status_ = std::string("Не удалось открыть предыдущий проект: ") + e.what();
            projectPath_ = projectPath_.parent_path() / ("New-game-" + std::to_string(SDL_GetPerformanceCounter()) + ".pxproj");
            doc_.dirty = true;
        }
    } else doc_.dirty = true;
    ImGui::StyleColorsDark(); auto& style = ImGui::GetStyle();
    style.WindowRounding = 5; style.FrameRounding = 4; style.GrabRounding = 4; style.FramePadding = {8, 5}; style.ItemSpacing = {8, 6};
    style.Colors[ImGuiCol_WindowBg] = {0.075f, 0.09f, 0.12f, 1};
    style.Colors[ImGuiCol_Header] = {0.14f, 0.25f, 0.32f, 1};
    style.Colors[ImGuiCol_Button] = {0.16f, 0.24f, 0.3f, 1};
    style.Colors[ImGuiCol_ButtonHovered] = {0.22f, 0.4f, 0.46f, 1};
    style.Colors[ImGuiCol_CheckMark] = {0.35f, 0.85f, 0.7f, 1};
    style.Colors[ImGuiCol_SliderGrab] = {0.35f, 0.75f, 0.65f, 1};
}
void Application::mark(const char* name) { const auto a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax(); controls_[name] = {a.x, a.y, b.x, b.y}; }
bool Application::button(const char* label, const char* id) { const bool pressed = ImGui::Button(label); if (id) mark(id); return pressed; }
void Application::changed() { doc_.changed(); }
SpriteArt* Application::selectedArt() {
    if (doc_.project.art.empty()) return nullptr;
    art_ = std::min(art_, doc_.project.art.size() - 1);
    auto& a = doc_.project.art[art_]; frame_ = std::min(frame_, a.frames.size() - 1); return &a;
}
void Application::syncTextures() {
    if (textureRevision_ == doc_.revision) return;
    for (const auto& a : doc_.project.art) view_.sprites().put(a.key(), a.atlas());
    textureRevision_ = doc_.revision;
}
void Application::open(const std::filesystem::path& path) { auto p = studio::load(path); doc_.reset(std::move(p)); projectPath_ = path; selection_.reset(); art_ = frame_ = 0; textureRevision_ = 0; status_ = "Проект открыт: " + path.u8string(); }
bool Application::save() {
    try { doc_.commit(); studio::save(doc_.project, projectPath_); doc_.dirty = false; status_ = "Сохранено: " + projectPath_.u8string(); return true; }
    catch (const std::exception& e) { status_ = std::string("Ошибка сохранения: ") + e.what(); return false; }
}
bool Application::requestQuit() { if (!doc_.dirty) return true; dialog_ = "unsaved-quit"; return false; }
void Application::fileDialog(const char* action) {
    dialog_ = action;
    auto p = projectPath_;
    if (dialog_ == "new") p = p.parent_path() / "New-game.pxproj";
    if (dialog_ == "import") p = p.parent_path() / "sprite.png";
    if (dialog_ == "export") p = p.parent_path() / "sprite-strip.png";
    std::snprintf(path_, sizeof(path_), "%s", p.u8string().c_str());
}
void Application::layout() {
    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({viewport->WorkPos.x, viewport->WorkPos.y + 48});
    ImGui::SetNextWindowSize({viewport->WorkSize.x, viewport->WorkSize.y - 48});
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Рабочий стол", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);
    const auto dock = ImGui::GetID("PixelStudioDock");
    if (!ImGui::DockBuilderGetNode(dock) || resetLayout_) {
        ImGui::DockBuilderRemoveNode(dock); ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dock, {viewport->WorkSize.x, viewport->WorkSize.y - 48});
        auto center = dock; const auto left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.16f, nullptr, &center);
        const auto right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.27f, nullptr, &center);
        const auto bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.27f, nullptr, &center);
        auto resource = left; const auto scene = ImGui::DockBuilderSplitNode(resource, ImGuiDir_Up, 0.58f, nullptr, &resource);
        ImGui::DockBuilderDockWindow("Сцена", scene); ImGui::DockBuilderDockWindow("Ресурсы", resource);
        ImGui::DockBuilderDockWindow("Свойства", right); ImGui::DockBuilderDockWindow("Рабочая область", center);
        ImGui::DockBuilderDockWindow("Таймлайн", bottom); ImGui::DockBuilderFinish(dock); resetLayout_ = false;
    }
    ImGui::DockSpace(dock, {0, 0}); ImGui::End(); ImGui::PopStyleVar();
}
void Application::toolbar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Проект")) {
            if (ImGui::MenuItem("Новый пример", nullptr, false, !doc_.runtime)) { if (doc_.dirty) dialog_ = "unsaved-new"; else fileDialog("new"); }
            if (ImGui::MenuItem("Открыть...", "Ctrl+O", false, !doc_.runtime)) { if (doc_.dirty) dialog_ = "unsaved-open"; else fileDialog("open"); }
            if (ImGui::MenuItem("Сохранить", "Ctrl+S", false, !doc_.runtime)) save();
            if (ImGui::MenuItem("Сохранить как...", nullptr, false, !doc_.runtime)) fileDialog("save");
            ImGui::Separator();
            if (ImGui::MenuItem("Импорт PNG...", nullptr, false, !doc_.runtime)) fileDialog("import");
            if (ImGui::MenuItem("Экспорт спрайта PNG...", nullptr, false, !doc_.runtime && selectedArt())) fileDialog("export");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Вид")) {
            if (ImGui::MenuItem("Вернуть расположение панелей")) resetLayout_ = true;
            ImGui::MenuItem("Сетка", "G", &grid_); ImGui::MenuItem("Предыдущий кадр", nullptr, &onion_);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Справка")) { ImGui::TextUnformatted("Уровень: выбор и перенос мышью; материалы рисуются кистью."); ImGui::TextUnformatted("Спрайты: ЛКМ — рисовать, ПКМ — стирать; Ctrl+Z/Y — история."); ImGui::TextUnformatted("Игра: щёлкните по уровню, A/D — ходьба, пробел — прыжок, E — действие."); ImGui::EndMenu(); }
        ImGui::TextDisabled("  PixelStudio 0.7"); ImGui::EndMainMenuBar();
    }
    const auto* v = ImGui::GetMainViewport(); ImGui::SetNextWindowPos(v->WorkPos); ImGui::SetNextWindowSize({v->WorkSize.x, 48});
    ImGui::Begin("Инструменты", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking);
    ImGui::BeginDisabled(bool(doc_.runtime));
    if (button("Сохранить", "save")) save(); ImGui::SameLine();
    ImGui::BeginDisabled(!doc_.canUndo()); if (button("Отмена", "undo")) doc_.undo(); ImGui::EndDisabled(); ImGui::SameLine();
    ImGui::BeginDisabled(!doc_.canRedo()); if (button("Повтор", "redo")) doc_.redo(); ImGui::EndDisabled(); ImGui::EndDisabled(); ImGui::SameLine();
    if (!doc_.runtime) {
        if (button("Играть", "play")) { doc_.start(false); accumulator_ = 0; gameFocus_ = true; requestedWorkspace_ = 0; }
        ImGui::SameLine(); if (button("Симуляция")) { doc_.start(true); accumulator_ = 0; requestedWorkspace_ = 0; }
    } else {
        if (button("Стоп", "stop")) { doc_.stop(); accumulator_ = 0; }
        ImGui::SameLine(); if (button(doc_.paused ? "Продолжить" : "Пауза")) { doc_.paused = !doc_.paused; accumulator_ = 0; }
        ImGui::SameLine(); if (button("Шаг")) { doc_.paused = true; doc_.step(); }
    }
    ImGui::SameLine(); ImGui::TextDisabled("%s%s", doc_.project.name.c_str(), doc_.dirty ? " *" : "");
    ImGui::End();
}
void Application::scenePanel() {
    ImGui::Begin("Сцена"); ImGui::BeginDisabled(bool(doc_.runtime));
    auto projectName = doc_.project.name; bool renamed = false;
    textField("Проект", projectName, renamed, 128);
    if (renamed && !projectName.empty()) { doc_.beginChange(); doc_.project.name = projectName; changed(); }
    ImGui::TextDisabled("Объекты уровня");
    if (ImGui::Selectable("Игрок — точка появления", !selection_)) { selection_.reset(); requestedWorkspace_ = 0; }
    for (std::size_t i = 0; i < doc_.project.game.scene.size(); ++i) {
        const auto& o = doc_.project.game.scene.object(static_cast<pixel::ObjectId>(i)); if (!o.active) continue;
        const auto label = (o.name.empty() ? "Объект" : o.name) + "##object" + std::to_string(i);
        if (ImGui::Selectable(label.c_str(), selection_ && *selection_ == i)) { selection_ = static_cast<pixel::ObjectId>(i); visual_ = 0; requestedWorkspace_ = 0; }
    }
    ImGui::EndDisabled(); ImGui::Separator(); ImGui::TextDisabled("Видимость слоёв");
    for (int i = 0; i < 4; ++i) ImGui::Checkbox(layers[i], &view_.layerVisible[static_cast<std::size_t>(i)]);
    ImGui::Separator(); ImGui::TextWrapped("%s", status_.c_str()); ImGui::End();
}
void Application::place(const char* preset) {
    try { selection_ = doc_.place(preset, {view_.camera().position().x + 46, std::string(preset) == "tree" ? 55.0f : 50.0f}, art_);
    visual_ = 0; requestedWorkspace_ = 0; status_ = "Объект добавлен. Перетащите его мышью или задайте координаты.";
    } catch (const std::exception& e) { status_ = e.what(); }
}
void Application::assets() {
    ImGui::Begin("Ресурсы"); ImGui::BeginDisabled(bool(doc_.runtime));
    ImGui::TextDisabled("Добавить на уровень");
    if (button("Дерево")) place("tree"); ImGui::SameLine(); if (button("Дверь")) place("door");
    if (button("Сундук")) place("chest");
    ImGui::Separator(); ImGui::TextDisabled("Спрайты проекта");
    for (std::size_t i = 0; i < doc_.project.art.size(); ++i) {
        auto& a = doc_.project.art[i];
        if (ImGui::Selectable((a.name + "##art" + std::to_string(a.id)).c_str(), art_ == i)) { art_ = i; frame_ = 0; requestedWorkspace_ = 1; }
        if (ImGui::BeginDragDropSource()) { ImGui::SetDragDropPayload("SPRITE_ASSET", &i, sizeof(i)); ImGui::TextUnformatted(a.name.c_str()); ImGui::EndDragDropSource(); }
    }
    if (selectedArt() && button("Разместить спрайт", "place-sprite")) place("sprite");
    ImGui::Separator(); ImGui::SetNextItemWidth(65); ImGui::InputInt("W", &newWidth_, 0); ImGui::SameLine(); ImGui::SetNextItemWidth(65); ImGui::InputInt("H", &newHeight_, 0);
    newWidth_ = std::clamp(newWidth_, 1, 128); newHeight_ = std::clamp(newHeight_, 1, 128);
    ImGui::BeginDisabled(doc_.project.art.size() >= 32);
    if (button("Новый спрайт", "new-sprite")) { doc_.addArt(newWidth_, newHeight_); art_ = doc_.project.art.size() - 1; frame_ = 0; requestedWorkspace_ = 1; }
    if (button("Импорт PNG")) fileDialog("import");
    ImGui::EndDisabled();
    ImGui::EndDisabled(); ImGui::End();
}
void Application::imagePreview(const SpriteArt& a, std::size_t f, ImVec2 size) {
    if (!view_.sprites().contains(a.key())) { ImGui::Dummy(size); return; }
    const auto& t = view_.sprites().get(a.key()); const float n = static_cast<float>(a.frames.size());
    ImGui::Image(texture(t.image.get()), size, {float(f) / n, 0}, {float(f + 1) / n, 1});
}
void Application::properties() {
    ImGui::Begin("Свойства"); ImGui::BeginDisabled(bool(doc_.runtime));
    if (workspace_ == 1 && selectedArt()) {
        auto& a = *selectedArt(); auto name = a.name; bool edited = false;
        textField("Имя", name, edited, 128);
        if (edited && !name.empty()) { doc_.beginChange(); a.name = name; changed(); }
        ImGui::TextDisabled("%d × %d px · %zu кадров", a.frames[0].width(), a.frames[0].height(), a.frames.size());
        ImGui::SeparatorText("Инструмент");
        const char* tools[] = {"Карандаш", "Ластик", "Заливка", "Пипетка"};
        for (int i = 0; i < 4; ++i) { if (i % 2) ImGui::SameLine(); ImGui::RadioButton(tools[i], &tool_, i); }
        ImGui::SliderInt("Радиус", &pixelRadius_, 0, 8);
        ImGui::ColorEdit4("Цвет", color_, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf);
        constexpr pixel::Color palette[] = {{29,32,44,255},{55,65,82,255},{107,123,145,255},{225,235,232,255},
            {80,49,37,255},{150,89,47,255},{215,153,77,255},{255,221,129,255},
            {34,75,58,255},{54,125,74,255},{116,180,78,255},{190,218,99,255},
            {39,77,117,255},{58,142,195,255},{174,79,96,255},{211,145,166,255}};
        for (int i = 0; i < 16; ++i) {
            ImGui::PushID(i); if (i % 4) ImGui::SameLine(); const auto c = palette[i];
            if (ImGui::ColorButton("##palette", {c.r / 255.f, c.g / 255.f, c.b / 255.f, 1}, 0, {30, 24})) {
                color_[0] = c.r / 255.f; color_[1] = c.g / 255.f; color_[2] = c.b / 255.f; color_[3] = 1;
            }
            ImGui::PopID();
        }
        ImGui::Checkbox("Сетка", &grid_); ImGui::Checkbox("Предыдущий кадр", &onion_);
        if (button("Отразить по горизонтали")) {
            doc_.beginChange(); auto& im = a.frames[frame_];
            for (int y = 0; y < im.height(); ++y) for (int x = 0; x < im.width() / 2; ++x) std::swap(im.at(x,y), im.at(im.width()-1-x,y)); changed(); doc_.commit();
        }
        if (button("Отразить по вертикали")) {
            doc_.beginChange(); auto& im = a.frames[frame_];
            for (int y = 0; y < im.height() / 2; ++y) for (int x = 0; x < im.width(); ++x) std::swap(im.at(x,y), im.at(x,im.height()-1-y)); changed(); doc_.commit();
        }
        if (button("Экспорт PNG-ленты")) fileDialog("export");
    } else if (selection_ && *selection_ < doc_.project.game.scene.size()) {
        auto object = doc_.project.game.scene.object(*selection_); bool edited = false;
        textField("Имя", object.name, edited, 128);
        edited |= ImGui::DragFloat2("Положение", &object.position.x, 0.125f, -256, 256, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        if (object.action == "door" || object.action == "chest") {
            bool state = object.state;
            if (ImGui::Checkbox("Открыт изначально", &state)) { doc_.beginChange(); doc_.project.game.setObjectOpen(*selection_, state); changed(); object = doc_.project.game.scene.object(*selection_); }
            ImGui::TextDisabled("Действие: %s", object.action == "door" ? "дверь" : "сундук");
        }
        if (!object.visuals.empty()) {
            ImGui::SeparatorText("Изображение");
            visual_ = std::clamp(visual_, 0, int(object.visuals.size() - 1));
            if (object.visuals.size() > 1) ImGui::SliderInt("Часть", &visual_, 0, int(object.visuals.size() - 1));
            auto& v = object.visuals[static_cast<std::size_t>(visual_)]; int layer = static_cast<int>(v.layer);
            if (ImGui::Combo("Слой", &layer, layers, 4)) { v.layer = static_cast<pixel::RenderLayer>(layer); edited = true; }
            edited |= ImGui::InputInt("Порядок", &v.order);
            edited |= ImGui::Checkbox("Видимая часть", &v.visible);
            edited |= ImGui::DragFloat2("Смещение", &v.bounds.x, .125f, -256, 256, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            edited |= ImGui::DragFloat2("Размер", &v.bounds.width, .125f, .125f, 256, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            edited |= ImGui::SliderFloat("Параллакс", &v.parallax, 0, 2, "%.2f");
            float c[] = {v.color.r / 255.f, v.color.g / 255.f, v.color.b / 255.f, v.color.a / 255.f};
            if (ImGui::ColorEdit4("Оттенок", c)) { v.color = {std::uint8_t(c[0]*255),std::uint8_t(c[1]*255),std::uint8_t(c[2]*255),std::uint8_t(c[3]*255)}; edited = true; }
        }
        ImGui::SeparatorText("Физика"); bool collider = bool(object.collider);
        if (ImGui::Checkbox("Коллайдер", &collider)) { object.collider = collider ? std::optional<pixel::Collider>(pixel::Collider{}) : std::nullopt; edited = true; }
        if (object.collider) {
            edited |= ImGui::DragFloat2("Смещение##коллизия", &object.collider->bounds.x, .125f, -256, 256, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            edited |= ImGui::DragFloat2("Размер##коллизия", &object.collider->bounds.width, .125f, .125f, 256, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        }
        if (object.interaction) { ImGui::SeparatorText("Взаимодействие"); edited |= ImGui::Checkbox("Доступно", &object.interaction->enabled); edited |= ImGui::SliderFloat("Дальность", &object.interaction->reach, .5f, 12); }
        if (edited) { doc_.beginChange(); doc_.project.game.scene.replace(*selection_, std::move(object)); changed(); }
        if (button("Дублировать объект")) {
            doc_.beginChange(); auto copy = doc_.project.game.scene.object(*selection_); copy.position.x += 3;
            selection_ = doc_.project.game.scene.add(copy); changed(); doc_.commit();
        }
        ImGui::SameLine();
        if (button("Удалить")) { doc_.beginChange(); doc_.project.game.scene.erase(*selection_); selection_.reset(); changed(); doc_.commit(); }
    } else {
        ImGui::SeparatorText("Точка появления");
        auto& game = doc_.project.game; float p[] = {game.x, game.y};
        if (ImGui::DragFloat2("Координаты игрока", p, .125f, 0, 126, "%.2f", ImGuiSliderFlags_AlwaysClamp)) {
            doc_.beginChange(); game.x = std::clamp(p[0], 0.f, 126.f); game.y = std::clamp(p[1], 0.f, 69.f); game.velocityY = 0; game.grounded = false; changed();
        }
        if (game.blocked(game.x, game.y)) ImGui::TextWrapped("Точка появления пересекает препятствие. Перенесите её в свободное место.");
    }
    ImGui::EndDisabled();
    if (auto* a = selectedArt()) {
        ImGui::SeparatorText("Предпросмотр анимации");
        const auto f = previewPlaying_ ? a->clip().frameAt(previewTime_) : std::min(frame_, a->frames.size()-1);
        const float scale = std::min(160.f / a->frames[0].width(), 160.f / a->frames[0].height());
        imagePreview(*a, f, {a->frames[0].width()*scale,a->frames[0].height()*scale});
        ImGui::TextDisabled("Кадр %zu / %zu", f + 1, a->frames.size());
    }
    ImGui::End();
}
void Application::canvas() {
    auto* a = selectedArt(); if (!a) { ImGui::TextWrapped("Создайте спрайт в панели ресурсов или импортируйте PNG."); return; }
    ImGui::SliderFloat("Масштаб", &zoom_, 3, 32, "%.0f×"); zoom_ = std::round(zoom_);
    ImGui::TextDisabled("%s · кадр %zu · ЛКМ рисует, ПКМ стирает", a->name.c_str(), frame_+1);
    ImGui::BeginChild("PixelCanvas", {0, 0}, ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
    auto& image = a->frames[frame_]; const auto origin = ImGui::GetCursorScreenPos(); const float z = zoom_;
    const ImVec2 size{image.width()*z,image.height()*z};
    ImGui::InvisibleButton("##canvas", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight); mark("canvas");
    const bool hovered = ImGui::IsItemHovered(), active = ImGui::IsItemActive(); const auto mouse = ImGui::GetIO().MousePos;
    auto* draw = ImGui::GetWindowDrawList();
    const auto clipMin = ImGui::GetWindowPos(), clipSize = ImGui::GetWindowSize();
    const pixel::Image* previous = onion_ && frame_ > 0 ? &a->frames[frame_-1] : nullptr;
    for (int y = 0; y < image.height(); ++y) for (int x = 0; x < image.width(); ++x) {
        ImVec2 p{origin.x+x*z, origin.y+y*z}, q{p.x+z,p.y+z};
        if (q.x < clipMin.x || q.y < clipMin.y || p.x > clipMin.x+clipSize.x || p.y > clipMin.y+clipSize.y) continue;
        draw->AddRectFilled(p,q, (x/4+y/4)%2 ? IM_COL32(58,65,78,255) : IM_COL32(43,49,61,255));
        if (previous && previous->at(x,y).a) draw->AddRectFilled(p,q,rgba(previous->at(x,y),.28f));
        if (image.at(x,y).a) draw->AddRectFilled(p,q,rgba(image.at(x,y)));
    }
    if (grid_ && z >= 8) {
        for (int x=0; x<=image.width(); ++x) draw->AddLine({origin.x+x*z,origin.y},{origin.x+x*z,origin.y+size.y},IM_COL32(16,20,28,80));
        for (int y=0; y<=image.height(); ++y) draw->AddLine({origin.x,origin.y+y*z},{origin.x+size.x,origin.y+y*z},IM_COL32(16,20,28,80));
    }
    const int x = static_cast<int>(std::floor((mouse.x-origin.x)/z)), y = static_cast<int>(std::floor((mouse.y-origin.y)/z));
    const bool inside = x>=0 && y>=0 && x<image.width() && y<image.height();
    if (!doc_.runtime && inside && (hovered || active)) {
        draw->AddRect({origin.x+x*z,origin.y+y*z},{origin.x+(x+1)*z,origin.y+(y+1)*z},IM_COL32(255,220,122,255));
        const bool left = ImGui::IsMouseDown(0), right = ImGui::IsMouseDown(1);
        if ((left || right) && active) {
            if (tool_ == 3 && !right) {
                const auto c = image.at(x,y); color_[0]=c.r/255.f;color_[1]=c.g/255.f;color_[2]=c.b/255.f;color_[3]=c.a/255.f;
            } else {
                const pixel::Color c = (right || tool_==1) ? pixel::Color{0,0,0,0} : pixel::Color{std::uint8_t(color_[0]*255),std::uint8_t(color_[1]*255),std::uint8_t(color_[2]*255),std::uint8_t(color_[3]*255)};
                if (tool_ == 2 && !right) { if (ImGui::IsMouseClicked(0)) { doc_.beginChange(); image.floodFill(x,y,c); changed(); } }
                else { doc_.beginChange(); const auto from = lastPixel_.value_or(std::make_pair(x,y)); image.line(from.first,from.second,x,y,c,pixelRadius_); changed(); lastPixel_=std::make_pair(x,y); }
            }
        }
    }
    if (!inside || (!ImGui::IsMouseDown(0) && !ImGui::IsMouseDown(1))) lastPixel_.reset();
    ImGui::EndChild();
}
void Application::level() {
    const char* tools[] = {"Выбор", "Материалы", "Влажность"}; ImGui::SetNextItemWidth(140); ImGui::Combo("Инструмент", &levelTool_, tools, 3); ImGui::SameLine();
    float camera = view_.camera().position().x; ImGui::SetNextItemWidth(130);
    if (ImGui::SliderFloat("Камера", &camera, 0, 32, "%.1f")) view_.camera().moveTo({camera,0});
    if (levelTool_ != 0 && !doc_.runtime) {
        const char* materials[] = {"Воздух", "Камень", "Земля", "Песок", "Вода", "Газ"}; ImGui::SetNextItemWidth(115); ImGui::Combo("Материал", &brush_, materials,6); ImGui::SameLine();
        ImGui::SetNextItemWidth(100); ImGui::SliderInt("Кисть", &brushRadius_,0,4);
        if (levelTool_==2) { ImGui::SetNextItemWidth(180); ImGui::SliderFloat("Влажность",&moisture_,0,1); }
        else if (brush_==4 || brush_==5) { ImGui::SetNextItemWidth(180); ImGui::SliderFloat("Количество",&fluidAmount_,.01f,brush_==4 ? 1.f : 5.f); }
    }
    const auto avail = ImGui::GetContentRegionAvail(); const float factor = std::max(.1f,std::min(avail.x/768.f,(avail.y-24)/576.f));
    const ImVec2 size{768*factor,576*factor}, origin=ImGui::GetCursorScreenPos();
    ImGui::Image(texture(target_.get()),size); mark("level");
    const bool hover=ImGui::IsItemHovered();
    if (ImGui::BeginDragDropTarget()) {
        if (const auto* payload=ImGui::AcceptDragDropPayload("SPRITE_ASSET")) {
            const auto index=*static_cast<const std::size_t*>(payload->Data);
            if (!doc_.runtime && index<doc_.project.art.size()) { const auto m=ImGui::GetIO().MousePos; const auto p=view_.mouseToWorld(int((m.x-origin.x)/factor),int((m.y-origin.y)/factor)); selection_=doc_.place("sprite",p,index); }
        }
        ImGui::EndDragDropTarget();
    }
    const auto mouse=ImGui::GetIO().MousePos;
    const auto world=view_.mouseToWorld(int((mouse.x-origin.x)/factor),int((mouse.y-origin.y)/factor));
    if (hover && ImGui::IsMouseClicked(0)) gameFocus_=true;
    if (!doc_.runtime && hover && levelTool_==0 && ImGui::IsMouseClicked(0)) {
        selection_.reset();
        auto list=doc_.project.game.scene.drawList();
        for (auto it=list.rbegin(); it!=list.rend(); ++it) {
            const auto& v=it->visual; if (!view_.layerVisible[static_cast<std::size_t>(v.layer)]) continue;
            const auto camera=view_.camera().position(); auto bounds=v.bounds; bounds.x -= camera.x*v.parallax; bounds.y -= camera.y*v.parallax;
            const auto screen=pixel::Vec2{world.x-camera.x,world.y-camera.y};
            if (screen.x>=bounds.x && screen.x<=bounds.x+bounds.width && screen.y>=bounds.y && screen.y<=bounds.y+bounds.height) { selection_=it->object; break; }
        }
        if (selection_) { draggingObject_=true;dragStart_=world;objectStart_=doc_.project.game.scene.object(*selection_).position;visual_=0; }
    }
    if (!doc_.runtime && draggingObject_ && selection_ && hover && ImGui::IsMouseDragging(0)) {
        doc_.beginChange(); doc_.project.game.scene.setPosition(*selection_,{std::round((objectStart_.x+world.x-dragStart_.x)*8)/8,std::round((objectStart_.y+world.y-dragStart_.y)*8)/8}); changed();
    }
    if (!ImGui::IsMouseDown(0)) draggingObject_=false;
    if (!doc_.runtime && levelTool_!=0 && hover && (ImGui::IsMouseDown(0)||ImGui::IsMouseDown(1))) {
        const int cx=int(std::floor(world.x)),cy=int(std::floor(world.y));
        if (cx>=0 && cy>=0 && cx<128 && cy<71) {
            doc_.beginChange(); const auto from=lastPixel_.value_or(std::make_pair(cx,cy)); const int steps=std::max(std::abs(cx-from.first),std::abs(cy-from.second));
            auto& game=doc_.project.game;
            for(int s=0;s<=steps;++s) {
                const int bx=steps ? from.first+(cx-from.first)*s/steps : cx, by=steps ? from.second+(cy-from.second)*s/steps : cy;
                for(int y=std::max(0,by-brushRadius_);y<=std::min(70,by+brushRadius_);++y) for(int x=std::max(0,bx-brushRadius_);x<=std::min(127,bx+brushRadius_);++x) {
                    if(levelTool_==2) { if(game.world.material(x,y)==pixel::Material::Soil) game.world.setMoisture(x,y,moisture_); }
                    else {
                        const auto material=ImGui::IsMouseDown(1) ? pixel::Material::Air : static_cast<pixel::Material>(brush_);
                        const bool solid=material==pixel::Material::Stone||material==pixel::Material::Soil||material==pixel::Material::Sand;
                        if(solid && pixel::overlaps({float(x),float(y),1,1},{game.x,game.y,2,3})) continue;
                        if(material==pixel::Material::Water||material==pixel::Material::Gas) game.world.setFluid(x,y,material,std::clamp(double(fluidAmount_),.01,material==pixel::Material::Water?1.0:5.0));
                        else game.world.set(x,y,material);
                    }
                }
            }
            changed();lastPixel_=std::make_pair(cx,cy);
        }
    } else if (!ImGui::IsMouseDown(0)&&!ImGui::IsMouseDown(1)) lastPixel_.reset();
    auto* draw=ImGui::GetWindowDrawList();
    if (!doc_.runtime && selection_ && *selection_<doc_.project.game.scene.size()) {
        const auto& o=doc_.project.game.scene.object(*selection_);
        for(const auto& v:o.visuals) {
            if(!v.visible)continue; const auto p=view_.camera().worldToScreen({o.position.x+v.bounds.x,o.position.y+v.bounds.y},v.parallax);
            draw->AddRect({origin.x+p.x*8*factor,origin.y+p.y*8*factor},{origin.x+(p.x+v.bounds.width)*8*factor,origin.y+(p.y+v.bounds.height)*8*factor},IM_COL32(255,218,115,255));
        }
    }
    if(doc_.runtime) ImGui::TextDisabled("%s",doc_.simulationOnly ? "Симуляция мира · исходный уровень не меняется" : "A/D — ходьба · пробел — прыжок · E — действие · Стоп вернёт исходный уровень");
    else if(hover && world.x>=0&&world.x<128&&world.y>=0&&world.y<72) {
        const auto& w=doc_.project.game.world;int x=int(world.x),y=int(world.y);
        ImGui::TextDisabled("Клетка %d, %d · вода %.2f · газ %.2f · влажность %.0f%%",x,y,w.water(x,y),w.gas(x,y),w.ground(x,y).moisture*100);
    }
}
void Application::workspace() {
    ImGui::Begin("Рабочая область");
    const int requested = requestedWorkspace_; requestedWorkspace_ = -1;
    if (ImGui::BeginTabBar("WorkspaceTabs")) {
        if (ImGui::BeginTabItem("Уровень", nullptr, requested==0 ? ImGuiTabItemFlags_SetSelected : 0)) { mark("level-tab");workspace_=0;level();ImGui::EndTabItem(); } else mark("level-tab");
        if (ImGui::BeginTabItem("Спрайты", nullptr, requested==1 ? ImGuiTabItemFlags_SetSelected : 0)) { mark("sprite-tab");workspace_=1;canvas();ImGui::EndTabItem(); } else mark("sprite-tab");
        ImGui::EndTabBar();
    }
    ImGui::End();
}
void Application::timeline() {
    ImGui::Begin("Таймлайн"); auto* a=selectedArt();
    if(!a){ImGui::TextUnformatted("Создайте спрайт для покадровой анимации.");ImGui::End();return;}
    if(button(previewPlaying_?"Остановить просмотр":"Воспроизвести", "preview-play")){previewPlaying_=!previewPlaying_;previewTime_=0;}
    ImGui::SameLine(); ImGui::BeginDisabled(bool(doc_.runtime));
    bool loop=a->loop;if(ImGui::Checkbox("Цикл",&loop)){doc_.beginChange();a->loop=loop;changed();}
    ImGui::SameLine();int ms=static_cast<int>(a->durations[frame_]);ImGui::SetNextItemWidth(90);
    if(ImGui::DragInt("мс",&ms,1,1,60000,"%d",ImGuiSliderFlags_AlwaysClamp)){doc_.beginChange();a->durations[frame_]=std::uint32_t(std::clamp(ms,1,60000));changed();}
    ImGui::BeginDisabled(a->frames.size()>=32);
    if(button("Пустой кадр", "blank-frame")){doc_.duplicateFrame(art_,frame_,true);++frame_;}
    ImGui::SameLine();if(button("Копия кадра", "duplicate-frame")){doc_.duplicateFrame(art_,frame_);++frame_;}
    ImGui::EndDisabled();ImGui::SameLine();
    ImGui::BeginDisabled(a->frames.size()==1);if(button("Удалить кадр")){doc_.removeFrame(art_,frame_);frame_=std::min(frame_,a->frames.size()-1);}ImGui::EndDisabled();
    ImGui::SameLine();ImGui::BeginDisabled(frame_==0);if(button("<")){doc_.moveFrame(art_,frame_,frame_-1);--frame_;}ImGui::EndDisabled();ImGui::SameLine();
    ImGui::BeginDisabled(frame_+1>=a->frames.size());if(button(">")){doc_.moveFrame(art_,frame_,frame_+1);++frame_;}ImGui::EndDisabled();
    ImGui::EndDisabled();
    ImGui::BeginChild("Frames",{0,0},ImGuiChildFlags_None,ImGuiWindowFlags_HorizontalScrollbar);
    auto* draw=ImGui::GetWindowDrawList();double elapsed=0;const auto playing=a->clip().frameAt(previewTime_);
    for(std::size_t f=0;f<a->frames.size();++f){
        if(f)ImGui::SameLine();ImGui::PushID(int(f));
        const float width=std::clamp(a->durations[f]*.5f,92.f,220.f);const auto p=ImGui::GetCursorScreenPos();
        if(ImGui::InvisibleButton("frame",{width,92})){frame_=f;previewPlaying_=false;requestedWorkspace_=1;}
        mark(("frame-"+std::to_string(f)).c_str());
        draw->AddRectFilled(p,{p.x+width,p.y+92},f==frame_?IM_COL32(47,79,93,255):IM_COL32(30,39,51,255),4);
        draw->AddText({p.x+6,p.y+4},IM_COL32(185,201,215,255),(std::to_string(int(elapsed))+" мс").c_str());
        const auto* t=view_.sprites().contains(a->key()) ? &view_.sprites().get(a->key()) : nullptr;const float n=float(a->frames.size());
        if (t) draw->AddImage(texture(t->image.get()),{p.x+width/2-20,p.y+25},{p.x+width/2+20,p.y+65},{float(f)/n,0},{float(f+1)/n,1});
        draw->AddText({p.x+6,p.y+72},IM_COL32(221,228,231,255),(std::to_string(f+1)+" · "+std::to_string(a->durations[f])+" мс").c_str());
        if(f==frame_)draw->AddRect(p,{p.x+width,p.y+92},IM_COL32(105,204,181,255),4,0,2);
        if(previewPlaying_&&f==playing)draw->AddLine({p.x+width/2,p.y},{p.x+width/2,p.y+92},IM_COL32(255,206,108,255),2);
        if(!doc_.runtime&&ImGui::BeginDragDropSource()){int source=int(f);ImGui::SetDragDropPayload("ANIMATION_FRAME",&source,sizeof(source));ImGui::Text("Кадр %zu",f+1);ImGui::EndDragDropSource();}
        if(!doc_.runtime&&ImGui::BeginDragDropTarget()){
            if(const auto* payload=ImGui::AcceptDragDropPayload("ANIMATION_FRAME")){const auto source=*static_cast<const int*>(payload->Data);doc_.moveFrame(art_,std::size_t(source),f);frame_=f;}
            ImGui::EndDragDropTarget();
        }
        elapsed+=a->durations[f];ImGui::PopID();
    }
    ImGui::EndChild();ImGui::End();
}
void Application::dialogs(){
    if(dialog_.rfind("unsaved-",0)==0){
        ImGui::OpenPopup("Несохранённые изменения");
        if(ImGui::BeginPopupModal("Несохранённые изменения",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
            ImGui::TextUnformatted("Сохранить изменения в проекте?");
            auto proceed=[&](){
                const auto action=dialog_;dialog_.clear();ImGui::CloseCurrentPopup();
                if(action=="unsaved-quit"){quit_=true;doc_.dirty=false;SDL_Event event{};event.type=SDL_QUIT;SDL_PushEvent(&event);}
                else if(action=="unsaved-new")fileDialog("new");
                else if(action=="unsaved-open")fileDialog("open");
            };
            if(button("Сохранить и продолжить")){if(save())proceed();}
            ImGui::SameLine();if(button("Не сохранять"))proceed();ImGui::SameLine();
            if(button("Отмена##unsaved")){dialog_.clear();ImGui::CloseCurrentPopup();}
            ImGui::EndPopup();
        }
        return;
    }
    if(dialog_.empty())return;
    ImGui::OpenPopup("Файл проекта или спрайта");
    if(ImGui::BeginPopupModal("Файл проекта или спрайта",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
        const auto action=dialog_;
        const char* title=action=="new"?"Создать новый проект .pxproj":action=="open"?"Открыть проект .pxproj":action=="save"?"Сохранить проект .pxproj":action=="import"?"Импорт PNG / горизонтальной ленты":"Экспорт кадров в горизонтальную PNG-ленту";
        ImGui::TextUnformatted(title);ImGui::SetNextItemWidth(620);ImGui::InputText("Путь",path_,sizeof(path_));
        if(action=="import"){ImGui::InputInt("Кадров по горизонтали",&importColumns_);importColumns_=std::clamp(importColumns_,1,32);ImGui::TextDisabled("Размер одного кадра — до 128 × 128 px.");}
        if(action=="export")ImGui::TextDisabled("Длительности и порядок кадров хранятся в проекте .pxproj.");
        if(button("Выполнить")){
            try{
                const auto file=std::filesystem::u8path(path_);if(file.empty())throw std::runtime_error("Enter a file path");
                if(action=="new") {
                    if(std::filesystem::exists(file))throw std::runtime_error("This file already exists; choose a new project filename");
                    auto p=Project::example();studio::save(p,file);doc_.reset(std::move(p));projectPath_=file;selection_.reset();art_=frame_=0;status_="Создан новый проект: "+file.u8string();
                } else if(action=="open")open(file);
                else if(action=="save"){studio::save(doc_.project,file);projectPath_=file;doc_.dirty=false;status_="Проект сохранён: "+file.u8string();}
                else if(action=="import"){
                    if(doc_.project.art.size()>=32)throw std::runtime_error("Sprite asset limit reached");
                    auto a=importPng(file,importColumns_,doc_.project.nextArtId);doc_.beginChange();++doc_.project.nextArtId;doc_.project.art.push_back(std::move(a));changed();doc_.commit();art_=doc_.project.art.size()-1;frame_=0;requestedWorkspace_=1;
                    status_="PNG импортирован: "+file.u8string();
                }else if(auto* a=selectedArt()){a->atlas().savePng(file);status_="PNG-лента экспортирована: "+file.u8string();}
                dialog_.clear();ImGui::CloseCurrentPopup();
            }catch(const std::exception& e){status_=std::string("Ошибка: ")+e.what();}
        }
        ImGui::SameLine();if(button("Отмена##file")){dialog_.clear();ImGui::CloseCurrentPopup();}
        ImGui::TextWrapped("%s",status_.c_str());ImGui::EndPopup();
    }
}
void Application::frame(double dt){
    if(!std::isfinite(dt)||dt<0)throw std::invalid_argument("Invalid editor frame time");
    dt=std::min(dt,.1);selectedArt();
    if(selection_&&(*selection_>=doc_.project.game.scene.size()||!doc_.project.game.scene.object(*selection_).active))selection_.reset();
    syncTextures();
    ImGui_ImplSDLRenderer2_NewFrame();ImGui_ImplSDL2_NewFrame();ImGui::GetIO().DeltaTime=static_cast<float>(std::max(.001,dt));ImGui::NewFrame();
    if(previewPlaying_)previewTime_+=dt;
    const bool text=ImGui::GetIO().WantTextInput;
    if(!text&&ImGui::GetIO().KeyCtrl&&!doc_.runtime){
        if(ImGui::IsKeyPressed(ImGuiKey_S))save();
        if(ImGui::IsKeyPressed(ImGuiKey_O)){if(doc_.dirty)dialog_="unsaved-open";else fileDialog("open");}
        if(ImGui::IsKeyPressed(ImGuiKey_Z))doc_.undo();if(ImGui::IsKeyPressed(ImGuiKey_Y))doc_.redo();
    }
    if(!text&&ImGui::IsKeyPressed(ImGuiKey_G))grid_=!grid_;
    int direction=0;
    if(doc_.runtime){
        if(!text&&gameFocus_){direction=int(ImGui::IsKeyDown(ImGuiKey_D)||ImGui::IsKeyDown(ImGuiKey_RightArrow))-int(ImGui::IsKeyDown(ImGuiKey_A)||ImGui::IsKeyDown(ImGuiKey_LeftArrow));if(ImGui::IsKeyPressed(ImGuiKey_E))doc_.runtime->interact();}
        if(!doc_.paused){accumulator_+=dt;bool jump=!text&&gameFocus_&&ImGui::IsKeyPressed(ImGuiKey_Space);
            while(accumulator_>=1.0/60){doc_.step(direction,jump);jump=false;accumulator_-=1.0/60;}}
    }
    SDL_SetRenderTarget(renderer_,target_.get());SDL_RenderSetViewport(renderer_,nullptr);
    view_.showHints=bool(doc_.runtime);view_.draw(doc_.runtime?*doc_.runtime:doc_.project.game,false,direction,doc_.runtime?doc_.playTime:previewTime_);
    SDL_SetRenderTarget(renderer_,nullptr);SDL_RenderSetViewport(renderer_,nullptr);
    controls_.clear();toolbar();layout();scenePanel();assets();properties();workspace();timeline();dialogs();
    if(!ImGui::IsMouseDown(0)&&!ImGui::IsMouseDown(1)&&!ImGui::IsAnyItemActive())doc_.commit();
    const std::string title="PixelStudio 0.7 — "+doc_.project.name+(doc_.dirty?" *":"");SDL_SetWindowTitle(window_,title.c_str());
    ImGui::Render();SDL_SetRenderDrawColor(renderer_,18,23,31,255);SDL_RenderClear(renderer_);
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(),renderer_);SDL_RenderPresent(renderer_);
}
void Application::screenshot(const std::filesystem::path& file){
    int w,h;if(SDL_GetRendererOutputSize(renderer_,&w,&h)!=0)throw std::runtime_error("Cannot get editor output size");
    std::vector<unsigned char> pixels(static_cast<std::size_t>(w)*h*4);
    if(SDL_RenderReadPixels(renderer_,nullptr,SDL_PIXELFORMAT_RGBA32,pixels.data(),w*4)!=0)throw std::runtime_error("Cannot read editor screenshot");
    pixel::Image image(w,h);for(int y=0;y<h;++y)for(int x=0;x<w;++x){const auto i=(static_cast<std::size_t>(y)*w+x)*4;image.at(x,y)={pixels[i],pixels[i+1],pixels[i+2],pixels[i+3]};}image.savePng(file);
}
void Application::smoke(){
    doc_.reset(Project::example()); selection_ = doc_.project.game.gate;
    auto check=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    for(int i=0;i<4;++i)frame(1.0/60);
    auto click=[&](const std::string& name){
        const auto it=controls_.find(name);if(it==controls_.end())throw std::runtime_error("Missing editor control: "+name);
        const auto p=it->second;auto& io=ImGui::GetIO();io.AddMousePosEvent((p.x+p.z)/2,(p.y+p.w)/2);frame(1.0/60);
        io.AddMouseButtonEvent(0,true);frame(1.0/60);io.AddMouseButtonEvent(0,false);frame(1.0/60);frame(1.0/60);
    };
    click("sprite-tab");check(workspace_==1,"Sprite workspace tab failed");
    const auto initialAssets=doc_.project.art.size(); click("new-sprite"); check(doc_.project.art.size()==initialAssets+1,"New canvas creation failed");
    frame_=0;previewPlaying_=false;auto* a=selectedArt();const auto original=a->frames[0].at(1,1);
    const auto c=controls_.at("canvas");auto& io=ImGui::GetIO();io.AddMousePosEvent(c.x+zoom_*1.5f,c.y+zoom_*1.5f);frame(1.0/60);
    io.AddMouseButtonEvent(0,true);frame(1.0/60);io.AddMouseButtonEvent(0,false);frame(1.0/60);
    check(a->frames[0].at(1,1).a!=original.a,"Canvas pencil failed");
    const auto painted=a->frames[0].at(1,1);click("undo");a=selectedArt();check(a->frames[0].at(1,1).a==original.a,"Canvas undo failed");
    click("redo");a=selectedArt();check(a->frames[0].at(1,1).a==painted.a,"Canvas redo failed");
    const auto n=a->frames.size();click("duplicate-frame");a=selectedArt();check(a->frames.size()==n+1&&a->frames[frame_]==a->frames[frame_-1],"Timeline frame duplication failed");
    click("frame-0");check(frame_==0,"Timeline scrubbing failed");
    click("place-sprite"); check(selection_ && doc_.project.game.scene.object(*selection_).visuals[0].sprite->asset==selectedArt()->key(),"Placing edited sprite failed");
    click("level-tab"); levelTool_=1; brush_=1; brushRadius_=0; frame(1.0/60);
    const auto v=controls_.at("level");const float k=(v.z-v.x)/768.f;
    io.AddMousePosEvent(v.x+40.5f*8*k,v.y+20.5f*8*k);frame(1.0/60);
    io.AddMouseButtonEvent(0,true);frame(1.0/60);io.AddMouseButtonEvent(0,false);frame(1.0/60);
    check(doc_.project.game.world.material(40,20)==pixel::Material::Stone,"Level material brush failed");
    click("undo");check(doc_.project.game.world.material(40,20)==pixel::Material::Air,"Level brush undo failed");levelTool_=0;
    const auto sceneBefore=doc_.project.game.x;click("play");check(bool(doc_.runtime),"Editor play failed");
    doc_.step(1,false);click("stop");check(!doc_.runtime&&doc_.project.game.x==sceneBefore,"Stop modified source scene");
    const auto directory=std::filesystem::temp_directory_path()/std::filesystem::u8path("PixelStudio-test-"+std::to_string(SDL_GetPerformanceCounter()));
    try{
        projectPath_=directory/std::filesystem::u8path("Проект с пробелами.pxproj");click("save");check(!doc_.dirty,"Save button failed");
        const auto saved=studio::load(projectPath_);check(saved.art[art_].frames==doc_.project.art[art_].frames,"Saved canvas differs");
        const auto file=directory/"sprite.png";a=selectedArt();a->atlas().savePng(file);check(pixel::Image::loadPng(file)==a->atlas(),"PNG export failed");
        std::filesystem::remove_all(directory);
    }catch(...){std::filesystem::remove_all(directory);throw;}
    std::cout<<"Editor UI smoke checks passed: dock panels, new sprite/instance, canvas pencil, undo/redo, timeline duplicate/scrub, level brush, play/stop, Unicode project save, PNG export\n";
}
}
