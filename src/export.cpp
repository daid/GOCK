#include "export.h"
#include <sp2/math/quaternion.h>
#include <sp2/graphics/color.h>
#include <algorithm>


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

void export_data(Mesh& mesh)
{
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
    for(auto index=0U; index<connectors.size(); index++) {
        auto& con = connectors[index];
        fprintf(f, "translate([%f*seperation, seperation, 0]) %% text(\"%dx\");\n", xoffset, con.amount);
        fprintf(f, "translate([%f*seperation, 0, 0]) connector(\"%c\") {\n", xoffset, 'A' + index);
        for(auto& c : con.connections) {
            fprintf(f, "  rotate([0, 0, %.2f]) rotate([0, %.2f, 0]) connection(type=%d);\n", c.yaw, c.pitch, c.type);
        }
        fprintf(f, "}\n");
        xoffset += 1.0;
    }

    auto len_max = *std::max_element(mesh.lengths.begin(), mesh.lengths.end());

    auto longest_possible_length = 240;
    auto connector_center_offset = 7;
    for(size_t idx=0; idx<mesh.lengths.size(); idx++) {
        fprintf(f, "echo(\"Type: %zd Length: %.1f\");\n", idx, mesh.lengths[idx] / len_max * (longest_possible_length + connector_center_offset) - connector_center_offset);
    }
    fclose(f);
    LOG(Debug, "Radius:", 1.0 / len_max * (longest_possible_length + connector_center_offset));

    f = fopen("export.svg", "wt");
    float svg_width = 290.0;
    float svg_height = 200.0;
    fprintf(f, "<svg width=\"%fmm\" height=\"%fmm\" viewBox=\"0 0 %f %f\" xmlns=\"http://www.w3.org/2000/svg\">", svg_width, svg_height, svg_width, svg_height);
    for(size_t idx=0; idx<mesh.lengths.size(); idx++) {
        auto len = mesh.lengths[idx] / len_max * (longest_possible_length + connector_center_offset) - connector_center_offset;
        auto color = sp::Color(sp::HsvColor(idx * 360 / mesh.lengths.size(), 80, 100));
        fprintf(f, "<rect x=\"1\" y=\"%f\" width=\"%f\" height=\"5\" style=\"fill:%s; stroke-width:.1; stroke: black\" />", idx * 15.0 + 5, len, color.toString().c_str());
        fprintf(f, "<text x=\"1.5\" y=\"%f\" font-size=\"2.8\" fill=\"black\">%dx %.1fmm</text>", idx * 15.0 + 9, mesh.lengths_count[idx], len);
        fprintf(f, "<path style=\"fill:%s; stroke-width:.2; stroke: black\" d=\"", color.toString().c_str());
        int sides_per_type[] = {6, 4, 5, 7, 3};
        int sides = sides_per_type[idx % 5];
        for(int n=0; n<sides; n++) {
            auto p = sp::Vector2d(0, 4).rotate((n + 0.5) * 360 / sides) + sp::Vector2d(10, 15 + idx*15);
            fprintf(f, "%c%f,%f", n ? 'L' : 'M', p.x, p.y);
        }
        fprintf(f, "z\"/>");
    }
    fprintf(f, "</svg>");
    fclose(f);
}