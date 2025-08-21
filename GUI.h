#pragma once
#include <SDL.h>
#include <SDL_ttf.h>
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <map>
#include <set>
#include <filesystem>
#include "Circuit.h"
#include "Analyzers.h"
#include "Pin.h"
#include "Wire.h"
// Oscilloscope.h included in GUI.cpp where needed

// Forward declaration
class Element;

// --- GUI Component Base Class ---
class GuiComponent {
public:
    virtual ~GuiComponent() = default;
    virtual void handleEvent(const SDL_Event& event) = 0;
    virtual void render(SDL_Renderer* renderer) = 0;
    virtual bool contains(int x, int y) const { return false; }
    virtual void setPosition(int x, int y) {}
    virtual void setSize(int w, int h) {}
    virtual SDL_Rect getBounds() const { return {0, 0, 0, 0}; }
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
public:
    enum class TabType { TRANSIENT, AC, PHASE };
private:
    SDL_Rect panel_rect;
    bool is_visible = false;
    TabType current_tab = TabType::AC;
    std::vector<std::unique_ptr<InputBox>> tran_inputs;
    std::vector<std::unique_ptr<InputBox>> ac_inputs;
    std::vector<std::unique_ptr<InputBox>> phase_inputs;
    std::function<void(TabType)> on_enter_callback = nullptr;
public:
    SimulationSettingsPanel(int x, int y, int w, int h, TTF_Font* font);
    void handleEvent(const SDL_Event& event) override;
    void render(SDL_Renderer* renderer) override;
    void toggleVisibility();
    bool isVisible() const { return is_visible; }
    bool contains(int x, int y) const { return is_visible && x >= panel_rect.x && x <= panel_rect.x + panel_rect.w && y >= panel_rect.y && y <= panel_rect.y + panel_rect.h; }
    double getTranTStart() const;
    double getTranTStep() const;
    double getTranTStop() const;
    std::string getACSource() const;
    double getACStartFreq() const;
    double getACStopFreq() const;
    int getACPoints() const;
    TabType getCurrentTab() const;
    void setOnEnterCallback(std::function<void(TabType)> callback) { on_enter_callback = callback; }
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
    bool show_node_names = true; // Show node names by default
    std::map<std::string, std::vector<double>> latest_results; // Store latest analysis results
    bool show_voltages = false; // Show voltage values on schematic
    
    // Node name hover detection
    struct NodeNameInfo {
        std::string node_id;
        SDL_Rect bounds;
        std::string display_name;
    };
    std::vector<NodeNameInfo> node_name_positions; // Track node name positions for hover detection
    
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
    void rebuildWiresFromCircuit(); // Rebuild GUI wires from circuit data
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
    void setShowVoltages(bool show) { show_voltages = show; }  // Toggle voltage display
    bool getShowVoltages() const { return show_voltages; }  // Get voltage display state
    void setLatestResults(const std::map<std::string, std::vector<double>>& results) { latest_results = results; }  // Set latest analysis results
    bool isPointOnWire(int x, int y);  // Check if point is near a wire
    
    // Node name hover detection methods
    std::string getNodeNameAt(int mouse_x, int mouse_y) const;  // Get node name at mouse position
    void clearNodeNamePositions();  // Clear node name position tracking
    void addNodeNamePosition(const std::string& node_id, const SDL_Rect& bounds, const std::string& display_name);  // Add node name position
    std::string getClosestNodeToPoint(int x, int y);  // Get closest node to a point
    double pointToLineDistance(int px, int py, int x1, int y1, int x2, int y2);  // Calculate distance from point to line

};

// SignalTrace removed - using Oscilloscope::Signal instead

// PlotMode and Cursor removed - using Oscilloscope instead

// PlotView removed - using Oscilloscope class instead

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
    virtual ~ComponentEditDialog() = default;
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
    
    // Scrolling support
    int scroll_offset = 0;
    int max_visible_items = 15; // Maximum number of signals visible at once
    int item_height = 22;
    bool is_scrolling = false;
    int scroll_start_y = 0;
    
public:
    ProbePanel(int x, int y, int w, int h, TTF_Font* font, std::function<void(const std::set<std::string>&)> on_apply);
    void setSignalsFromResults(const std::map<std::string, std::vector<double>>& results);
    const std::set<std::string>& getSelected() const { return selected; }
    void addSignal(const std::string& signal_name);
    void autoSelectVoltageSignals();
    void toggleVisibility() { is_visible = !is_visible; }
    bool isVisible() const { return is_visible; }
    void handleEvent(const SDL_Event& event) override;
    void render(SDL_Renderer* renderer) override;
    
private:
    void handleScroll(int mouse_y);
    void updateScrollLimits();
};

// --- TransientAnalysisDialog Implementation ---
class TransientAnalysisDialog : public GuiComponent {
private:
    std::unique_ptr<InputBox> tstart_input;
    std::unique_ptr<InputBox> tstep_input;
    std::unique_ptr<InputBox> tstop_input;
    bool is_visible = false;
    SDL_Rect dialog_rect;
    TTF_Font* font;
    std::function<void(double, double, double)> on_run;
    std::function<void()> on_cancel;

public:
    TransientAnalysisDialog(int x, int y, int w, int h, TTF_Font* font,
                          std::function<void(double, double, double)> run_callback,
                          std::function<void()> cancel_callback);
    void handleEvent(const SDL_Event& event) override;
    void render(SDL_Renderer* renderer) override;
    void show();
    void hide();
    bool isVisible() const { return is_visible; }
    
private:
    double parseTimeWithUnits(const std::string& time_str) const;
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
    TransientAnalysisDialog* tran_dialog = nullptr;

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
    
    // Latest transient parameters
    double latest_tstart = 0.0;
    double latest_tstep = 1e-5;
    double latest_tstop = 10e-3;
    
    // Save system
    std::string save_directory;
    std::string openFileDialog(const std::string& title, const std::string& defaultPath, const std::string& filter);
    std::string openLoadFileDialog(const std::string& title, const std::string& defaultPath, const std::string& filter);
    
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
    void onRunDCAnalysisClicked();
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
    void onShowTransientDialog();
    void runTransientAnalysis(double tstart, double tstep, double tstop);

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
