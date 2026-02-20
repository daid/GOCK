#include "mesh.h"

#include <sp2/math/vector3.h>
#include <algorithm>


int Mesh::add_vertex(sp::Vector3d v)
{
    for(size_t idx=0; idx<vertices.size(); idx++) {
        if ((v - vertices[idx].position).length() < EPSILON)
            return idx;
    }
    vertices.push_back({v});
    return vertices.size() - 1;
}

void Mesh::remove_if(std::function<bool(const Vertex& v)> func) {
    for(size_t idx=0; idx<vertices.size(); ) {
        if (func(vertices[idx])) {
            remove(idx);
        } else {
            idx++;
        }
    }
}

void Mesh::remove(size_t idx)
{
    for(auto& v : vertices) {
        v.edges.erase(std::remove_if(v.edges.begin(), v.edges.end(), [idx](auto& edge) { return edge.v_index == idx; }), v.edges.end());
        for(auto& e : v.edges)
            if (e.v_index >= idx)
                e.v_index -= 1;
    }
    vertices.erase(vertices.begin() + idx);
}

void Mesh::add_vertex_flipped(sp::Vector3d v)
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

void Mesh::build_edges()
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
    lengths_count.clear();
    for(auto& v : vertices) {
        for(auto& edge : v.edges) {
            auto length = (v.position - vertices[edge.v_index].position).length();
            auto it = std::find_if(lengths.begin(), lengths.end(), [length](auto len) { return std::abs(len - length) < EPSILON; });
            if (it != lengths.end()) {
                edge.type = it - lengths.begin();
                lengths_count[edge.type] += 1;
            } else {
                edge.type = lengths.size();
                lengths.push_back(length);
                lengths_count.push_back(1);
            }
        }
    }
    for(auto& count : lengths_count) count /= 2;
}

void Mesh::normalize() {
    for(auto& v : vertices)
        v.position = v.position.normalized();
}

Mesh Mesh::subdiv() {
    Mesh result;
    for(const auto& v : vertices) {
        result.add_vertex(v.position);
        for(const auto& edge : v.edges) {
            result.add_vertex((v.position + vertices[edge.v_index].position) * 0.5);
        }
    }
    return result;
}

std::shared_ptr<sp::MeshData> Mesh::create_mesh()
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
