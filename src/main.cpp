#include <volt-ui/VoltApp.h>
#include <imgui.h>

class CodeYoYoApp : public volt::App {
public:
    using volt::App::App;
protected:
    void OnCreate() override {
        // SetClearColor({0.12f, 0.12f, 0.15f, 1.0f});
    }

    void OnRender() override {
        
    }

    void OnEvent(const SDL_Event& event) override {
        // if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
        //     Quit();
        // }
    }

private:
    bool show_demo_ = false;
};

int main() {
    volt::AppConfig cfg;
    cfg.title = "CodeYoYo";
    cfg.width = 1280;
    cfg.height = 720;
    cfg.use_topbar = true;
    CodeYoYoApp app(cfg);
    return app.Run();
}
