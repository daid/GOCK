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


sp::io::Keybinding mouse_left{"MOUSE_LEFT", "pointer:1"};
sp::io::Keybinding mouse_x{"MOUSE_X", "mouse:x"};
sp::io::Keybinding mouse_y{"MOUSE_Y", "mouse:y"};


class Mesh
{
public:
    class Vertex
    {
    public:
        sp::Vector3d position;
        std::vector<size_t> edge;
    };

    int add_vertex(sp::Vector3d v)
    {
        for(size_t idx=0; idx<vertices.size(); idx++) {
            if ((v - vertices[idx].position).length() < 0.00001)
                return idx;
        }
        vertices.push_back({v});
        return vertices.size() - 1;
    }

    void remove_if(std::function<bool(const Vertex& v)> func) {
        for(size_t idx=0; idx<vertices.size(); ) {
            if (func(vertices[idx])) {
                remove(idx);
            } else {
                idx++;
            }
        }
    }

    void remove(size_t idx)
    {
        for(auto& v : vertices) {
            v.edge.erase(std::remove_if(v.edge.begin(), v.edge.end(), [idx](size_t i) { return i == idx; }), v.edge.end());
            for(auto& i : v.edge)
                if (i >= idx)
                    i -= 1;
        }
        vertices.erase(vertices.begin() + idx);
    }

    void add_vertex_flipped(sp::Vector3d v)
    {
        add_vertex(v);
        if (v.x > 0) {
            add_vertex({-v.x, v.y, v.z});
            if (v.y > 0) {
                add_vertex({-v.x, -v.y, v.z});
                if (v.z > 0) {
                    add_vertex({-v.x, -v.y, -v.z});
                }
            }
            if (v.z > 0) {
                add_vertex({-v.x, v.y, -v.z});
            }
        }
        if (v.y > 0) {
            add_vertex({ v.x,-v.y, v.z});
            if (v.z > 0) {
                add_vertex({ v.x,-v.y,-v.z});
            }
        }
        if (v.z > 0) {
            add_vertex({ v.x, v.y,-v.z});
        }
    }

    void build_edges()
    {
        for(size_t base_idx=0; base_idx<vertices.size(); base_idx++) {
            double closest_dist = std::numeric_limits<double>::max();
            for(size_t idx=0; idx<vertices.size(); idx++) {
                if (idx == base_idx) continue;
                closest_dist = std::min(closest_dist, (vertices[base_idx].position - vertices[idx].position).length());
            }
            for(size_t idx=0; idx<vertices.size(); idx++) {
                if (idx == base_idx) continue;
                if ((vertices[base_idx].position - vertices[idx].position).length() > closest_dist * 1.5) continue;
                vertices[base_idx].edge.push_back(idx);
            }
        }
    }

    void normalize() {
        for(auto& v : vertices)
            v.position = v.position.normalized();
    }

    Mesh subdiv() {
        Mesh result;
        for(const auto& v : vertices) {
            result.add_vertex(v.position);
            for(auto idx : v.edge) {
                result.add_vertex((v.position + vertices[idx].position) * 0.5);
            }
        }
        return result;
    }

    std::shared_ptr<sp::MeshData> create_mesh()
    {
        sp::MeshData::Vertices vertices;
        sp::MeshData::Indices indices;

        auto add = [&](sp::Vector3d p0, sp::Vector3d p1) {
            indices.push_back(vertices.size());
            indices.push_back(vertices.size()+1);
            indices.push_back(vertices.size()+2);

            indices.push_back(vertices.size());
            indices.push_back(vertices.size()+2);
            indices.push_back(vertices.size()+1);

            auto nf = sp::Vector3f(p0 + p1).normalized();

            vertices.push_back({{float(p0.x), float(p0.y), float(p0.z)}, nf});
            vertices.push_back({{float(p1.x), float(p1.y), float(p1.z)}, nf});
            auto n = (p1 - p0).normalized();
            auto s = p0.normalized().cross(n).normalized() * 0.03;
            auto p2 = p0 + s;
            vertices.push_back({{float(p2.x), float(p2.y), float(p2.z)}, nf});
        };
        for(const auto& v : this->vertices) {
            for(auto i : v.edge) {
                add(v.position, this->vertices[i].position);
            }
        }

        return sp::MeshData::create(std::move(vertices), std::move(indices));
    }

    std::vector<Vertex> vertices;
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

        Mesh mesh;
        if (false) {
            mesh.add_vertex_flipped({ 1, 1, 1});
            mesh.build_edges();
        }
        if (true) {
            auto phi = (1.0 + std::sqrt(5)) / 2.0;
            mesh.add_vertex_flipped({ 0.0, phi, 1.0});
            mesh.add_vertex_flipped({ phi, 1.0, 0.0});
            mesh.add_vertex_flipped({ 1.0, 0.0, phi});

            mesh.build_edges();
            mesh = mesh.subdiv();
            mesh.normalize();
            for(auto& v : mesh.vertices) {
                v.position = sp::Quaterniond::fromVectorToVector(sp::Vector3d(0, -phi, 1.0).normalized(), sp::Vector3d(0, 0, 1).normalized()) * v.position;
            }
            mesh.remove_if([](auto& v) {
                return v.position.z < -0.001;
            });
            mesh.build_edges();
        }

        {
            auto len_min = std::numeric_limits<double>::max();
            auto len_max = std::numeric_limits<double>::min();
            for(const auto& v : mesh.vertices) {
                for(auto i : v.edge) {
                    auto length = (v.position - mesh.vertices[i].position).length();
                    len_min = std::min(len_min, length);
                    len_max = std::max(len_max, length);
                }
            }

            for(const auto& v : mesh.vertices) {
                auto q = sp::Quaterniond::fromVectorToVector(v.position.normalized(), sp::Vector3d(0, 0, 1).normalized());
                
            }
        }

        auto n = new sp::Node(getRoot());
        n->render_data.type = sp::RenderData::Type::Normal;
        n->render_data.shader = sp::Shader::get("internal:color_shaded.shader");
        n->render_data.mesh = mesh.create_mesh();
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
