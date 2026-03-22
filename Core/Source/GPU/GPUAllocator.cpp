#include "GPUAllocator.h"
#include "glad.h"
#include "raylib.h"
#include <vector>

using namespace Mupfel;

static constexpr GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_MAP_READ_BIT;

GPUAllocator::Handle GPUAllocator::allocateGPUBuffer(uint32_t size)
{

	Handle new_handle;
	glGenBuffers(1, &new_handle.id);

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, new_handle.id);


	glBufferStorage(GL_SHADER_STORAGE_BUFFER, size, nullptr, flags);
	//glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);

	new_handle.mapped_ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, size, flags);

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	if (!new_handle.mapped_ptr)
	{
		TraceLog(LOG_ERROR, "Allocation of %u bytes of Shader Storage Buffer failed...", size);
	}

	new_handle.capacity = size;

	return new_handle;
}

void GPUAllocator::freeGPUBuffer(GPUAllocator::Handle& h)
{

	if (h.id != 0) {
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, h.id);
		if (h.mapped_ptr)
			glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
		glDeleteBuffers(1, &h.id);
	}

	h.id = 0;
	h.capacity = 0;
	h.mapped_ptr = nullptr;
}

void GPUAllocator::reallocateGPUBuffer(Handle& h, uint32_t new_size)
{
	if (h.capacity >= new_size)
	{
		return;
	}

	Handle newH = allocateGPUBuffer(new_size);

	/* Only copy if the previous buffer was not empty */
	if (h.capacity > 0)
	{
		std::vector<uint8_t> tmp;
		tmp.resize(h.capacity);

		std::memcpy(tmp.data(), h.mapped_ptr, h.capacity);
		/* Depending if the new buffer is larger or smaller, we choose the size */
		uint32_t min_cap = std::min<uint32_t>(h.capacity, newH.capacity);
		std::memcpy(newH.mapped_ptr, tmp.data(), min_cap);
	}

	//glCopyNamedBufferSubData(oldID, newH.id, 0, 0, oldBytes);

	freeGPUBuffer(h);
	h = newH;

}

void Mupfel::GPUAllocator::MemBarrier()
{
	//glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
}
