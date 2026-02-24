#include "process.h"
#include <sp2/math/quaternion.h>


void CreateMeshStep::process(Mesh& mesh)
{
    if (false) {
        mesh.add_vertex_flipped({ 1, 1, 1});
        mesh.build_edges();
    }
    if (true) {
        auto phi = (1.0 + std::sqrt(5)) / 2.0;
        mesh.add_vertex_flipped({ 0.0, phi, 1.0});
        mesh.add_vertex_flipped({ phi, 1.0, 0.0});
        mesh.add_vertex_flipped({ 1.0, 0.0, phi});

        for(auto& v : mesh.vertices) {
            v.position = sp::Quaterniond::fromVectorToVector(sp::Vector3d(0, -phi, 1.0).normalized(), sp::Vector3d(0, 0, 1).normalized()) * v.position;
        }
    }
}