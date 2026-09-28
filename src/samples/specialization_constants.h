#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "renderer/descriptor.h"
#include "renderer/texture.h"
#include "sample.h"

namespace glint {

class SpecializationConstants : public Sample {
 public:
  SpecializationConstants();

  void initSample(Window* window, Renderer* renderer) override;
  void update(float deltaTime) override;
  void render(VkCommandBuffer commandBuffer, uint32_t imageIndex) override;
  void cleanup() override;

 private:
  void updateUniformBuffers();
  void updateDynamicUniformBuffer();

 private:
  void prepareUniformBuffers();
  void setupDescriptors();

  std::unique_ptr<Mesh> m_Mesh;

  // Descriptor resources
  std::unique_ptr<DescriptorSetLayout> m_DescriptorSetLayout;
  std::unique_ptr<DescriptorPool> m_DescriptorPool;
  std::unique_ptr<Descriptor> m_Descriptor;

  std::vector<VkDescriptorSetLayoutBinding> m_Bindings;

  ////////

  // Transformation state
  float m_RotationAngle = 0.0f;
  glm::vec3 m_ModelPosition = glm::vec3(0.0f);
  glm::vec3 m_RotationAxis = glm::vec3(0.0f, 0.0f, 1.0f);

  struct UniformData {
    glm::mat4 projection;
    glm::mat4 modelView;
    glm::vec4 lightPos{0.0f, -2.0f, 1.0f, 0.0f};
  } uniformData;
  std::unique_ptr<UniformBuffer> m_viewUBO;

  static const uint32_t OBJECT_INSTANCES = 125;
  // Store random per-object rotations
  glm::vec3 rotations[OBJECT_INSTANCES];
  glm::vec3 rotationSpeeds[OBJECT_INSTANCES];

  float animationTimer{0.0f};
  size_t dynamicAlignment{0};
};

}  // namespace glint