#pragma once

#include "mesh.h"

class IProcessStep
{
public:
    virtual void process(Mesh& mesh) = 0;
};

class CreateMeshStep : public IProcessStep
{
public:
    void process(Mesh& mesh) override;
};