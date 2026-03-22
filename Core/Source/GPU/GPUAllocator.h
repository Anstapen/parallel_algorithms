#pragma once
#include <cstdint>

namespace Mupfel {

	template<typename T>
	class GPUVector;

	/**
	 * @brief This class wraps the OpenGL Shader Buffer operations.
	 */
	class GPUAllocator
	{
		template<typename T> friend class GPUVector;
	public:
		struct Handle {
			uint32_t id;
			uint32_t capacity;
			void* mapped_ptr;
		};
	private:
		static Handle allocateGPUBuffer(uint32_t size);
		static void freeGPUBuffer(Handle& h);
		static void reallocateGPUBuffer(Handle& h, uint32_t new_size);
		static void MemBarrier();
	};
}