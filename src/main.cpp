#include <sp2/engine.h>
#include <sp2/window.h>
#include <sp2/logging.h>
#include <sp2/io/resourceProvider.h>
#include <sp2/graphics/gui/scene.h>
#include <sp2/graphics/gui/theme.h>
#include <sp2/graphics/gui/loader.h>
#include <sp2/graphics/scene/graphicslayer.h>
#include <sp2/graphics/scene/basicnoderenderpass.h>
#include <sp2/graphics/scene/collisionrenderpass.h>
#include <sp2/graphics/textureManager.h>
#include <sp2/graphics/meshbuilder.h>
#include <sp2/collision/2d/box.h>
#include <sp2/scene/scene.h>
#include <sp2/scene/node.h>
#include <sp2/scene/camera.h>
#include <sp2/scene/tilemap.h>
#include <sp2/io/keybinding.h>
#include "mesh.h"
#include "process.h"
#include "export.h"


sp::io::Keybinding mouse_left{"MOUSE_LEFT", "pointer:1"};
sp::io::Keybinding mouse_x{"MOUSE_X", "mouse:x"};
sp::io::Keybinding mouse_y{"MOUSE_Y", "mouse:y"};

std::vector<std::unique_ptr<IProcessStep>> process;

class RainbowTexture : public sp::OpenGLTexture
{
public:
    RainbowTexture() : sp::OpenGLTexture(sp::Texture::Type::Static, "Rainbow") {
        sp::Image image{{128, 1}};
        for(int n=0; n<128; n++)
            image.getPtr()[n] = sp::Color(sp::HsvColor(n * 360 / 128, 100, 100)).toInt();
        setImage(std::move(image));
    }
};

class Scene : public sp::Scene
{
public:
    Scene()
    : sp::Scene("MAIN")
    {
        auto c = new sp::Camera(getRoot());
        c->setPerspective(60);
        setDefaultCamera(c);
        updateCameraPos();

        process.push_back(std::make_unique<CreateMeshStep>());

        Mesh mesh;
        for(auto& step : process) {
            step->process(mesh);
        }

        {
            mesh.build_edges();
            mesh = mesh.subdiv();
            mesh.normalize();
            mesh.remove_if([](auto& v) {
                return v.position.z < -EPSILON;
            });
            mesh.build_edges();
        }

        export_data(mesh);

        auto n = new sp::Node(getRoot());
        n->render_data.type = sp::RenderData::Type::Normal;
        n->render_data.shader = sp::Shader::get("internal:basic_shaded.shader");
        n->render_data.mesh = mesh.create_mesh();
        n->render_data.texture = new RainbowTexture();
    }

    void updateCameraPos()
    {
        auto p = sp::Vector3d(0, 0, 4);
        p = sp::Quaterniond::fromAxisAngle({1, 0, 0}, camera_pitch) * p;
        p = sp::Quaterniond::fromAxisAngle({0, 0, 1}, camera_yaw) * p;
        getCamera()->setPosition(p);
        getCamera()->setRotation(sp::Quaterniond::fromAxisAngle({0, 0, 1}, camera_yaw) * sp::Quaterniond::fromAxisAngle({1, 0, 0}, camera_pitch));
    }

    void onUpdate(float delta) override
    {
        if (mouse_left.get()) {
            camera_yaw -= mouse_x.getValue();
            camera_pitch -= mouse_y.getValue();
            camera_pitch = std::clamp(camera_pitch, 0.0, 180.0);
            updateCameraPos();
        }
    }

    double camera_yaw = 0;
    double camera_pitch = 45;
};

sp::P<sp::Window> window;

int main(int argc, char** argv)
{
    sp::P<sp::Engine> engine = new sp::Engine();

    //Create resource providers, so we can load things.
    sp::io::ResourceProvider::createDefault();

    //Disable or enable smooth filtering by default, enabling it gives nice smooth looks, but disabling it gives a more pixel art look.
    sp::texture_manager.setDefaultSmoothFiltering(false);

    //Create a window to render on, and our engine.
    window = new sp::Window(4.0/3.0);
#if !defined(DEBUG) && !defined(EMSCRIPTEN)
    window->setFullScreen(true);
#endif

    sp::gui::Theme::loadTheme("default", "gui/theme/basic.theme.txt");
    new sp::gui::Scene(sp::Vector2d(640, 480));

    sp::P<sp::SceneGraphicsLayer> scene_layer = new sp::SceneGraphicsLayer(1);
    scene_layer->addRenderPass(new sp::BasicNodeRenderPass());
#ifdef DEBUG
    scene_layer->addRenderPass(new sp::CollisionRenderPass());
#endif
    window->addLayer(scene_layer);

    new Scene();

    engine->run();

    return 0;
}
