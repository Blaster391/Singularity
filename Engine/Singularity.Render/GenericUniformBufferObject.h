#pragma once
#include <glm/matrix.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>

#include <Singularity.Core/CoreDeclare.h>

namespace Singularity
{
    namespace Render
    {

        struct GenericUniformBufferObject
        {
            glm::mat4 m_model = glm::mat4(1.0f);
            glm::mat4 m_view = glm::mat4(1.0f);
            glm::mat4 m_projection = glm::mat4(1.0f);
        };

    }
}