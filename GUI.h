#pragma once
#include <SDL.h>
#include <SDL_ttf.h>
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <map>
#include <set>
#include "Circuit.h"
#include "Analyzers.h"
#include "Pin.h"
#include "Wire.h"
#include "PlotCursor.h"

// Forward declaration
class Element;

// --- GUI Component Base Class ---
class GuiComponent {
public:
    virtual ~GuiComponent() = default;
    virtual void handleEvent(const SDL_Event& event) = 0;
    virtual void render(SDL_Renderer* renderer) = 0;
};

// --- Button Base Class ---
class Button : public GuiComponent {
protected:
    SDL_Rect rect;
    bool is_hovered;
    bool is_clicked;
public:
    Button(int x, int y, int w, int h);
    virtual ~Button() = default;
    void handleEvent(const SDL_Event& event) override;
    virtual void render(SDL_Renderer* renderer) = 0;
    virtual void doAction() = 0;
};

// --- ActionButton Class ---
class ActionButton : public Button {
private:
    std::string text;
    TTF_Font* font;
    SDL_Texture* text_texture;
    SDL_Rect text_rect;
    std::function<void()> action_callback;
    void createTextTexture(SDL_Renderer* renderer);
public:
    ActionButton(int x, int y, int w, int h, std::string button_text, TTF_Font* btn_font, std::function<void()> action);
    ~ActionButton();
    void doAction() override;
    void render(SDL_Renderer* renderer) override;
};

// --- InputBox Class ---
class InputBox : public GuiComponent {
private:
    SDL_Rect rect;
    std::string text;
    TTF_Font* font;
    bool is_active = false;
public:
    InputBox(int x, int y, int w, int h, TTF_Font* font, std::string default_text = "");
    void handleEvent(const SDL_Event& event) override;
    void render(SDL_Renderer* renderer) override;
    std::string getText() const;
    void setText(const std::string& new_text);
    TTF_Font* getFont() const { return font; }
};

// --- SimulationSettingsPanel Class ---
class SimulationSettingsPanel : public GuiComponent {
private:
    SDL_Rect panel_rect;
    bool is_visible = false;
    std::vector<std::unique_ptr<InputBox>> tran_inputs;
    std::vector<std::unique_ptr<InputBox>> ac_inputs;
    std::vector<std::unique_ptr<InputBox>> phase_inputs;
public:
    SimulationSettingsPanel(int x, int y, int w, int h, TTF_Font* font);
    void handleEvent(const SDL_Event& event) override;
    void render(SDL_Renderer* renderer) override;
    void toggleVisibility();
    bool isVisible() const { return is_visible; }
    bool contains(int x, int y) const { return is_visible && x >= panel_rect.x && x <= panel_rect.x + panel_rect.w && y >= panel_rect.y && y <= panel_rect.y + panel_rect.h; }
    double getTranTStep() const;
    double getTranTStop() const;
    std::string getACSource() const;
    double getACStartFreq() const;
    double getACStopFreq() const;
    int getACPoints() const;
};


// --- ComponentSelector Class ---
class ComponentSelector : public GuiComponent {
private:
    SDL_Rect panel_rect;
    std::vector<std::unique_ptr<ActionButton>> component_buttons;
    std::vector<std::unique_ptr<ActionButton>> source_buttons;
    std::vector<std::unique_ptr<ActionButton>> dependent_source_buttons;
    std::unique_ptr<ActionButton> wire_button; // NEW: Button for wire creation
    bool is_visible = false;
    bool show_sources = false;
    bool show_dependent_sources = false;
public:
    ComponentSelector(int x, int y, int w, int h, TTF_Font* font, std::function<void(const std::string&)> on_select);
    void handleEvent(const SDL_Event& event) override;
    void render(SDL_Renderer* renderer) override;
    void toggleVisibility();
    void toggleSourceMenu();
    void toggleDependentSourceMenu();
    bool isVisible() const { return is_visible; }
};

// --- SchematicView Class ---
class SchematicView : public GuiComponent {
private:
    SDL_Rect view_area;
    Circuit& circuit_backend;
    TTF_Font* font; // <-- NEW: Font for rendering labels
    const int GRID_SIZE = 15; // Reduced grid size for better component placement
    std::map<std::string, SDL_Texture*>& component_textures;
    std::vector<std::shared_ptr<Pin>> pins;
    // Persistent index of pins per element to avoid losing connections on refresh
    std::map<std::string, std::shared_ptr<Pin>> pin_index; // key: "<elem>.<pinNumber>"
    std::vector<std::shared_ptr<GuiWire>> wires;
    bool show_node_names = false;
    
    void drawElementSymbol(SDL_Renderer* renderer, const Element& elem);
    void drawNodeLabels(SDL_Renderer* renderer); // <-- NEW: Method to draw labels
    void drawPins(SDL_Renderer* renderer); // NEW: Draw pins on components
    void drawWires(SDL_Renderer* renderer); // NEW: Draw wires between pins
    double calculateOptimalScale(int original_w, int original_h); // <-- NEW: Helper for dynamic scaling
    
public:
    // Maintenance
    void updatePinPositions(); // NEW: Update pin positions when elements move
    void clearWires(); // NEW: Clear all GUI wires
    // Wire management
    void createWire(std::shared_ptr<Pin> start_pin, std::shared_ptr<Pin> end_pin);
    void createGuiWireOnly(std::shared_ptr<Pin> start_pin, std::shared_ptr<Pin> end_pin);
    void deleteWire(const std::string& wire_id);
    std::shared_ptr<Pin> getPinAt(int x, int y) const;
    std::shared_ptr<Pin> getPinNear(int x, int y, int hover_radius = 15) const;  // For hover detection
    void updatePinHoverStates(int mouse_x, int mouse_y, bool is_wire_mode = false, bool is_creating_wire = false);
    SchematicView(int x, int y, int w, int h, Circuit& circuit, std::map<std::string, SDL_Texture*>& textures, TTF_Font* font);
    void handleEvent(const SDL_Event& event) override;
    void render(SDL_Renderer* renderer) override;
    std::string getNodeAt(int mouse_x, int mouse_y);
    SDL_Point getNodePosition(const std::string& node_id);
    SDL_Point snapToGrid(int x, int y);  // Snap coordinates to grid
    std::string getNodeAtGridPos(int grid_x, int grid_y);  // Get node at grid position
    const SDL_Rect& getViewArea() const { return view_area; }  // Get view area for calculations
    void setShowNodeNames(bool show) { show_node_names = show; }  // Toggle node name display
};

// --- SignalTrace Struct (for PlotView) ---
struct SignalTrace {
    std::string name;
    std::vector<double> y_values;
    SDL_Color color;
};

enum class PlotMode {
    TRANSIENT,
    AC_MAGNITUDE,
    PHASE_MAGNITUDE
};

struct Cursor {
    bool visible = false;
    int screen_x = 0;
    double world_x = 0.0, world_y = 0.0;
};

// --- PlotView Class ---
class PlotView : public GuiComponent {
private:
    SDL_Rect view_area;
    TTF_Font* font;
    std::vector<double> x_values;
    std::vector<SignalTrace> signals;
    PlotMode current_mode = PlotMode::TRANSIENT;
    double offset_x = 0.0, offset_y = 0.0;
    double scale_x = 1.0, scale_y = 1.0;
    bool is_panning = false;
    SDL_Point pan_start_pos;
    Cursor cursor1, cursor2;
    CursorManager cursor_manager;
    
    // Dragging and resizing support
    bool is_dragging = false;
    bool is_resizing = false;
    int drag_start_x = 0;
    int drag_start_y = 0;
    int resize_handle_size = 10;

    SDL_Point toScreenCoords(double world_x, double world_y);
    SDL_Point toWorldCoords(int screen_x, int screen_y);
    void renderText(SDL_Renderer* renderer, const std::string& text, int x, int y, SDL_Color color);
    void updateCursorValue(Cursor& cursor);
    void drawAxisLabels(SDL_Renderer* renderer);
    void renderCursors(SDL_Renderer* renderer);
    void updateCursorManager();

public:
    PlotView(int x, int y, int w, int h, TTF_Font* font);
    void handleEvent(const SDL_Event& event) override;
    void render(SDL_Renderer* renderer) override;
    void setData(const std::vector<double>& time_points, const std::map<std::string, std::vector<double>>& analysis_results);
    void setDataFiltered(const std::vector<double>& time_points, const std::map<std::string, std::vector<double>>& analysis_results, const std::set<std::string>& selected_names);
    void setDataAC(const std::vector<double>& freq_points, const std::map<std::string, std::vector<Complex>>& ac_results);
    void setDataPhase(const std::vector<double>& phase_points, const std::map<std::string, std::vector<Complex>>& phase_results);
    void autoZoom();
};

// --- ComponentEditDialog Class ---
class ComponentEditDialog : public GuiComponent {
private:
    SDL_Rect dialog_rect;
    TTF_Font* font;
    bool is_visible = false;
    Element* target_element = nullptr;
    std::string current_value;
    InputBox* value_input = nullptr;
    std::vector<std::unique_ptr<InputBox>> param_inputs;
    std::vector<std::string> param_labels;
    std::function<void()> on_apply_callback;
    std::function<void()> on_cancel_callback;
    
public:
    ComponentEditDialog(int x, int y, int w, int h, TTF_Font* font, 
                       std::function<void()> on_apply, std::function<void()> on_cancel);
    void setTargetElement(Element* element);
    void show();
    void hide();
    bool isVisible() const { return is_visible; }
    void handleEvent(const SDL_Event& event) override;
    void render(SDL_Renderer* renderer) override;
};

// --- ProbePanel Class ---
class ProbePanel : public GuiComponent {
private:
    SDL_Rect panel_rect;
    TTF_Font* font;
    bool is_visible = false;
    std::vector<std::string> available_signals;
    std::set<std::string> selected;
    std::function<void(const std::set<std::string>&)> on_apply;
public:
    ProbePanel(int x, int y, int w, int h, TTF_Font* font, std::function<void(const std::set<std::string>&)> on_apply);
    void setSignalsFromResults(const std::map<std::string, std::vector<double>>& results);
    const std::set<std::string>& getSelected() const { return selected; }
    void toggleVisibility() { is_visible = !is_visible; }
    bool isVisible() const { return is_visible; }
    void handleEvent(const SDL_Event& event) override;
    void render(SDL_Renderer* renderer) override;
};

// --- GuiApplication Class ---
class GuiApplication {
private:
    bool is_running = true;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    TTF_Font* font = nullptr;
    std::unique_ptr<Circuit> circuit;
    std::vector<std::unique_ptr<GuiComponent>> ui_elements;
    std::map<std::string, SDL_Texture*> component_textures;

    ComponentSelector* component_selector = nullptr;
    SchematicView* schematic_view = nullptr;
    SimulationSettingsPanel* settings_panel = nullptr;
    ProbePanel* probe_panel = nullptr;
    ComponentEditDialog* edit_dialog = nullptr;

    std::string placing_component_type;
    int placement_step = 0;
    std::string node1, node2, ctrl_node1, ctrl_node2;

    bool is_drawing_wire = false;
    std::string wire_start_node;
    SDL_Point current_mouse_pos;
    SDL_Point wire_draw_start_pos;
    bool is_wire_drag_active = false;
    
    // Pin-based wire creation
    std::shared_ptr<Pin> wire_start_pin;
    bool is_creating_wire_from_pin = false;

    bool is_creating_subcircuit = false;
    bool is_labeling_node = false;

    // Latest analysis data for probes
    std::vector<double> latest_time_points;
    std::map<std::string, std::vector<double>> latest_tran_results;
    std::set<std::string> selected_signals;
    
    // Analysis and probe states
    bool analysis_completed = false;
    bool is_probe_mode = false;
    enum class ProbeType { VOLTAGE, CURRENT } current_probe_type = ProbeType::VOLTAGE;

    void initialize();
    void handleEvents();
    void render();
    void cleanup();
    SDL_Texture* loadTexture(const std::string& path);

    void onRunSimulationClicked();
    void onRunACAnalysisClicked();
    void onRunDCSweepClicked();
    void onRunPhaseAnalysisClicked();
    void onSaveProjectClicked();
    void onLoadProjectClicked();
    void onSaveSubcircuitClicked();
    void onToggleComponentSelector();
    void onToggleSettingsPanel();
    void onAddNodeLabel();
    void onToggleProbePanel();
    void onShowSignalMath();
    void onToggleCursors();
    void onQuitClicked();
    void toggleWireMode();
    void toggleProbeMode();
    void setProbeType(ProbeType type);
    void clearAllProbes();

    void selectComponentToPlace(const std::string& type);
    void handleSchematicClick(const SDL_Event& event);
    void handleProbeHover(int mouse_x, int mouse_y);
    void resetPlacementState();
    void renderComponentPreview(SDL_Renderer* renderer);
    
    // Wire creation methods
    void startWireFromPin(std::shared_ptr<Pin> pin);
    void finishWireToPin(std::shared_ptr<Pin> pin);
    void cancelWireCreation();

    // Undo/Redo and Reset
    std::vector<std::string> undo_stack;
    std::vector<std::string> redo_stack;
    void pushUndoSnapshot();
    void applySnapshot(const std::string& snapshot);

public:
    GuiApplication();
    ~GuiApplication();
    void run();
};
