#pragma once
#include "GPUAllocator.h"

namespace Mupfel {

	template<typename T>
	class GPUVector
	{
	public:
		GPUVector();
		virtual ~GPUVector();
		size_t size() const;
		void resize(size_t count, const T& val);
		T& operator[](size_t pos);
		const T& operator[](size_t pos) const;
		uint32_t GetSSBOID() const;
	private:
		size_t m_size;
		GPUAllocator::Handle h;
	};

	template<typename T>
	inline GPUVector<T>::GPUVector() : m_size(0)
	{
		h.capacity = 0;
		h.id = 0;
		h.mapped_ptr = nullptr;
	}

	template<typename T>
	inline GPUVector<T>::~GPUVector()
	{
		if (h.mapped_ptr != nullptr)
		{
			GPUAllocator::freeGPUBuffer(h);
		}
		
	}

	template<typename T>
	inline size_t GPUVector<T>::size() const
	{
		return m_size;
	}

	template<typename T>
	inline void GPUVector<T>::resize(size_t count, const T& val)
	{
		if (m_size >= count)
		{
			return;
		}

		size_t new_bytes = ((count * sizeof(T)) + 255) & ~255;

		/* Resize the GPU vector */
		GPUAllocator::reallocateGPUBuffer(h, new_bytes);

		/* Set the newly added entries to val */
		for (uint32_t i = m_size; i < count; i++)
		{
			this->operator[](i) = val;
		}

		/* Set new size */
		m_size = count;
	}

	template<typename T>
	inline T& GPUVector<T>::operator[](size_t pos)
	{
		T* ptr = static_cast<T*>(h.mapped_ptr);

		return ptr[pos];
	}

	template<typename T>
	inline const T& GPUVector<T>::operator[](size_t pos) const
	{
		T* ptr = static_cast<T*>(h.mapped_ptr);

		return ptr[pos];
	}

	template<typename T>
	inline uint32_t GPUVector<T>::GetSSBOID() const
	{
		return h.id;
	}

}


