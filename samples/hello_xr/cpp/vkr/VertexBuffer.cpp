#include "VertexBuffer.h"
#include "MemoryAllocator.h"
#include <stdexcept>

VertexBuffer::~VertexBuffer() {
  if (m_vkDevice != nullptr) {
    if (idxBuf != VK_NULL_HANDLE) {
      vkDestroyBuffer(m_vkDevice, idxBuf, nullptr);
    }
    if (idxMem != VK_NULL_HANDLE) {
      vkFreeMemory(m_vkDevice, idxMem, nullptr);
    }
    if (vtxBuf != VK_NULL_HANDLE) {
      vkDestroyBuffer(m_vkDevice, vtxBuf, nullptr);
    }
    if (vtxMem != VK_NULL_HANDLE) {
      vkFreeMemory(m_vkDevice, vtxMem, nullptr);
    }
  }
  idxBuf = VK_NULL_HANDLE;
  idxMem = VK_NULL_HANDLE;
  vtxBuf = VK_NULL_HANDLE;
  vtxMem = VK_NULL_HANDLE;
  bindDesc = {};
  attrDesc.clear();
  count = {0, 0};
  m_vkDevice = nullptr;
}

std::shared_ptr<VertexBuffer>
VertexBuffer::Create(VkDevice device, VkPhysicalDevice physicalDevice,
                     const std::vector<VkVertexInputAttributeDescription> &attr,
                     const Vertex *vertices, uint32_t vtxCount,
                     const uint16_t *indices, uint32_t idxCount) {
  auto ptr = std::shared_ptr<VertexBuffer>(new VertexBuffer);
  ptr->m_vkDevice = device;
  ptr->attrDesc = attr;

  auto memAllocator = MemoryAllocator::Create(physicalDevice, device);

  VkBufferCreateInfo bufInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bufInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  bufInfo.size = sizeof(uint16_t) * idxCount;
  if (vkCreateBuffer(ptr->m_vkDevice, &bufInfo, nullptr, &ptr->idxBuf) !=
      VK_SUCCESS) {
    throw std::runtime_error("vkCreateBuffer");
  }
  ptr->idxMem = memAllocator->AllocateBufferMemory(ptr->idxBuf);
  if (vkBindBufferMemory(ptr->m_vkDevice, ptr->idxBuf, ptr->idxMem, 0) !=
      VK_SUCCESS) {
    throw std::runtime_error("vkBindBufferMemory");
  }

  bufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  bufInfo.size = sizeof(Vertex) * vtxCount;
  if (vkCreateBuffer(ptr->m_vkDevice, &bufInfo, nullptr, &ptr->vtxBuf) !=
      VK_SUCCESS) {
    throw std::runtime_error("vkCreateBuffer");
  }
  ptr->vtxMem = memAllocator->AllocateBufferMemory(ptr->vtxBuf);
  if (vkBindBufferMemory(ptr->m_vkDevice, ptr->vtxBuf, ptr->vtxMem, 0) !=
      VK_SUCCESS) {
    throw std::runtime_error("vkBindBufferMemory");
  }

  ptr->bindDesc = {{
      .binding = 0,
      .stride = sizeof(Vertex),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  }};

  ptr->count = {idxCount, vtxCount};

  {
    uint16_t *map = nullptr;
    if (vkMapMemory(ptr->m_vkDevice, ptr->idxMem, 0,
                    sizeof(uint16_t) * idxCount, 0,
                    (void **)&map) != VK_SUCCESS) {
      throw std::runtime_error("vkMapMemory");
    }
    for (size_t i = 0; i < idxCount; ++i) {
      map[i] = indices[i];
    }
    vkUnmapMemory(ptr->m_vkDevice, ptr->idxMem);
  }

  {
    Vertex *map = nullptr;
    if (vkMapMemory(ptr->m_vkDevice, ptr->vtxMem, 0, sizeof(map[0]) * vtxCount,
                    0, (void **)&map) != VK_SUCCESS) {
      throw std::runtime_error("vkMapMemory");
    }
    for (size_t i = 0; i < vtxCount; ++i) {
      map[i] = vertices[i];
    }
    vkUnmapMemory(ptr->m_vkDevice, ptr->vtxMem);
  }

  return ptr;
}
