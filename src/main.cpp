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

#define EPSILON 0.001


sp::io::Keybinding mouse_left{"MOUSE_LEFT", "pointer:1"};
sp::io::Keybinding mouse_x{"MOUSE_X", "mouse:x"};
sp::io::Keybinding mouse_y{"MOUSE_Y", "mouse:y"};

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

class Mesh
{
public:
    class Edge
    {
    public:
        size_t v_index;
        int type = 0;
    };

    class Vertex
    {
    public:
        sp::Vector3d position;
        std::vector<Edge> edges;
    };

    int add_vertex(sp::Vector3d v)
    {
        for(size_t idx=0; idx<vertices.size(); idx++) {
            if ((v - vertices[idx].position).length() < EPSILON)
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
            v.edges.erase(std::remove_if(v.edges.begin(), v.edges.end(), [idx](auto& edge) { return edge.v_index == idx; }), v.edges.end());
            for(auto& e : v.edges)
                if (e.v_index >= idx)
                    e.v_index -= 1;
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
        for(auto& v : vertices) {
            v.edges.clear();
        }

        for(size_t base_idx=0; base_idx<vertices.size(); base_idx++) {
            double closest_dist = std::numeric_limits<double>::max();
            for(size_t idx=0; idx<vertices.size(); idx++) {
                if (idx == base_idx) continue;
                closest_dist = std::min(closest_dist, (vertices[base_idx].position - vertices[idx].position).length());
            }
            for(size_t idx=0; idx<vertices.size(); idx++) {
                if (idx == base_idx) continue;
                if ((vertices[base_idx].position - vertices[idx].position).length() > closest_dist * 1.5) continue;
                vertices[base_idx].edges.push_back({idx});
            }
        }

        lengths.clear();
        for(auto& v : vertices) {
            for(auto& edge : v.edges) {
                auto length = (v.position - vertices[edge.v_index].position).length();
                auto it = std::find_if(lengths.begin(), lengths.end(), [length](auto len) { return std::abs(len - length) < EPSILON; });
                if (it != lengths.end()) {
                    edge.type = it - lengths.begin();
                } else {
                    edge.type = lengths.size();
                    lengths.push_back(length);
                }
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
            for(const auto& edge : v.edges) {
                result.add_vertex((v.position + vertices[edge.v_index].position) * 0.5);
            }
        }
        return result;
    }

    std::shared_ptr<sp::MeshData> create_mesh()
    {
        sp::MeshData::Vertices vertices;
        sp::MeshData::Indices indices;

        auto add = [&](sp::Vector3d p0, sp::Vector3d p1, float u) {
            indices.push_back(vertices.size());
            indices.push_back(vertices.size()+1);
            indices.push_back(vertices.size()+2);

            indices.push_back(vertices.size());
            indices.push_back(vertices.size()+2);
            indices.push_back(vertices.size()+1);

            auto nf = sp::Vector3f(p0 + p1).normalized();

            vertices.push_back({{float(p0.x), float(p0.y), float(p0.z)}, nf, {u, 0}});
            vertices.push_back({{float(p1.x), float(p1.y), float(p1.z)}, nf, {u, 0}});
            auto n = (p1 - p0).normalized();
            auto s = p0.normalized().cross(n).normalized() * 0.03;
            auto p2 = p0 + s;
            vertices.push_back({{float(p2.x), float(p2.y), float(p2.z)}, nf, {u, 0}});
        };
        for(const auto& v : this->vertices) {
            for(const auto& edge : v.edges) {
                add(v.position, this->vertices[edge.v_index].position, float(edge.type) / float(lengths.size()));
            }
        }

        return sp::MeshData::create(std::move(vertices), std::move(indices));
    }

    std::vector<Vertex> vertices;
    std::vector<double> lengths;
};

class Connection
{
public:
    double yaw = 0.0;
    double pitch = 0.0;
    int type = 0;
};
class Connector
{
public:
    int amount = 0;
    std::vector<Connection> connections;

    bool equal(const Connector& other) const {
        if (connections.size() != other.connections.size()) return false;
        for(size_t idx=0; idx<connections.size(); idx++) {
            if (std::abs(connections[idx].yaw - other.connections[idx].yaw) > EPSILON) return false;
            if (std::abs(connections[idx].pitch - other.connections[idx].pitch) > EPSILON) return false;
        }
        return true;
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
                return v.position.z < -EPSILON;
            });
            mesh.build_edges();
        }

        {
            auto len_min = std::numeric_limits<double>::max();
            auto len_max = std::numeric_limits<double>::min();
            for(const auto& v : mesh.vertices) {
                for(auto edge : v.edges) {
                    auto length = (v.position - mesh.vertices[edge.v_index].position).length();
                    len_min = std::min(len_min, length);
                    len_max = std::max(len_max, length);
                }
            }

            std::vector<Connector> connectors;
            for(const auto& v : mesh.vertices) {
                auto q = sp::Quaterniond::fromVectorToVector(v.position.normalized(), sp::Vector3d(0, 0, 1).normalized());
                auto p0 = q * v.position;
                Connector connector;
                for(const auto& edge : v.edges) {
                    auto p1 = q * mesh.vertices[edge.v_index].position;
                    auto n = (p1 - p0).normalized();
                    auto yaw = std::atan2(n.x, n.y) / sp::pi * 180.0;
                    auto pitch = std::acos(-n.z) / sp::pi * 180.0;
                    connector.connections.push_back({yaw, pitch, edge.type});
                }
                std::sort(connector.connections.begin(), connector.connections.end(), [](const auto& a, const auto& b) {
                    return a.yaw < b.yaw;
                });

                auto best_idx0 = 0;
                auto best_score = std::numeric_limits<double>::max();
                for(size_t idx0=0; idx0<connector.connections.size(); idx0++) {
                    auto yaw0 = connector.connections[idx0].yaw;
                    auto score = 0.0;
                    for(const auto& c : connector.connections) {
                        auto yaw = c.yaw - yaw0;
                        if (yaw < 0) yaw += 360.0;
                        score += yaw * c.pitch;
                    }
                    if (score < best_score) {
                        best_score = score;
                        best_idx0 = idx0;
                    }
                }
                auto yaw0 = connector.connections[best_idx0].yaw;
                for(auto& c : connector.connections) {
                    c.yaw -= yaw0;
                    if (c.yaw < 0) c.yaw += 360.0;
                }
                std::sort(connector.connections.begin(), connector.connections.end(), [](const auto& a, const auto& b) {
                    return a.yaw < b.yaw;
                });
                
                bool done = false;
                for(auto& c : connectors) {
                    if (c.equal(connector)) {
                        c.amount += 1;
                        done = true;
                        break;
                    }
                }
                if (!done) {
                    connectors.push_back(connector);
                    connectors.back().amount = 1;
                }
            }

            float xoffset = -0.5 * double(connectors.size() - 1);
            FILE* f = fopen("export.scad", "wt");
            for(auto& con : connectors) {
                fprintf(f, "translate([%f*seperation, seperation, 0]) %% text(\"%dx\");\n", xoffset, con.amount);
                fprintf(f, "translate([%f*seperation, 0, 0]) connector() {\n", xoffset);
                for(auto& c : con.connections) {
                    fprintf(f, "  rotate([0, 0, %.2f]) rotate([0, %.2f, 0]) connection(type=%d);\n", c.yaw, c.pitch, c.type);
                }
                fprintf(f, "}\n");
                xoffset += 1.0;
            }

            auto longest_possible_length = 240;
            auto connector_center_offset = 7;
            for(size_t idx=0; idx<mesh.lengths.size(); idx++) {
                fprintf(f, "echo(\"Type: %zd Length: %.1f\");\n", idx, mesh.lengths[idx] / len_max * (longest_possible_length + connector_center_offset) - connector_center_offset);
            }
            fclose(f);
            LOG(Debug, "Radius:", 1.0 / len_max * (longest_possible_length + connector_center_offset));
        }

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
