#pragma once

#define EPSILON 0.001

#include <sp2/math/vector3.h>
#include <sp2/graphics/meshdata.h>
#include <functional>


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

    int add_vertex(sp::Vector3d v);
    void remove_if(std::function<bool(const Vertex& v)> func);
    void remove(size_t idx);
    void add_vertex_flipped(sp::Vector3d v);
    void build_edges();
    void normalize();
    Mesh subdiv();
    std::shared_ptr<sp::MeshData> create_mesh();

    std::vector<Vertex> vertices;
    std::vector<double> lengths;
    std::vector<int> lengths_count;
};
