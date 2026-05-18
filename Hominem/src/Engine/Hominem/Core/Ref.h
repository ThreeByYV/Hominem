#pragma once

#include <memory>
#include <atomic>
#include <concepts>
#include <cstdio>
#include <cstdint>

namespace Hominem {

	class RefCounted
	{
	public:
		void AddRef() const {
			uint32_t prev = m_RefCount.fetch_add(1, std::memory_order_relaxed);
			if (m_TraceRefs)
				fprintf(stderr, "[RefTrace] AddRef  %p  %u -> %u\n", (void*)this, prev, prev + 1);
		}
		void Release() const {
			uint32_t prev = m_RefCount.fetch_sub(1, std::memory_order_acq_rel);
			if (m_TraceRefs)
				fprintf(stderr, "[RefTrace] Release %p  %u -> %u\n", (void*)this, prev, prev - 1);
			if (prev == 1)
				delete this;
		}
		uint32_t RefCount() const { return m_RefCount.load(std::memory_order_relaxed); }
		void EnableRefTrace() const {
			m_TraceRefs = true;
			fprintf(stderr, "[RefTrace] Enabled on %p\n", (void*)this);
		}

	protected:
		RefCounted() = default;
		virtual ~RefCounted() = default;

	private:
		mutable std::atomic<uint32_t> m_RefCount{ 0 };
		mutable bool                  m_TraceRefs = false;
	};

	// Intrusive smart pointer. T must derive from RefCounted.
	template<typename T>
	class IntrusiveRef
	{
		template<typename U> friend class IntrusiveRef;
	public:
		IntrusiveRef() = default;
		IntrusiveRef(std::nullptr_t) {}
		explicit IntrusiveRef(T* p) : m_Ptr(p) { if (m_Ptr) m_Ptr->AddRef(); }
		IntrusiveRef(const IntrusiveRef& o) : m_Ptr(o.m_Ptr) { if (m_Ptr) m_Ptr->AddRef(); }
		IntrusiveRef(IntrusiveRef&& o) noexcept : m_Ptr(o.m_Ptr) { o.m_Ptr = nullptr; }
		// Converting constructors — allow IntrusiveRef<Derived> → IntrusiveRef<Base>
		template<typename U>
		IntrusiveRef(const IntrusiveRef<U>& o) : m_Ptr(o.m_Ptr) { if (m_Ptr) m_Ptr->AddRef(); }
		template<typename U>
		IntrusiveRef(IntrusiveRef<U>&& o) noexcept : m_Ptr(o.m_Ptr) { o.m_Ptr = nullptr; }
		~IntrusiveRef() { if (m_Ptr) m_Ptr->Release(); }

		IntrusiveRef& operator=(const IntrusiveRef& o) {
			if (this != &o) {
				if (o.m_Ptr) o.m_Ptr->AddRef();
				if (m_Ptr)   m_Ptr->Release();
				m_Ptr = o.m_Ptr;
			}
			return *this;
		}
		IntrusiveRef& operator=(IntrusiveRef&& o) noexcept {
			if (m_Ptr) m_Ptr->Release();
			m_Ptr = o.m_Ptr; o.m_Ptr = nullptr;
			return *this;
		}
		IntrusiveRef& operator=(std::nullptr_t) {
			if (m_Ptr) m_Ptr->Release();
			m_Ptr = nullptr;
			return *this;
		}

		T* get()          const { return m_Ptr; }
		T* operator->()   const { return m_Ptr; }
		T& operator*()    const { return *m_Ptr; }
		explicit operator bool() const { return m_Ptr != nullptr; }

		bool operator==(const IntrusiveRef& o) const { return m_Ptr == o.m_Ptr; }
		bool operator!=(const IntrusiveRef& o) const { return m_Ptr != o.m_Ptr; }
		bool operator==(std::nullptr_t)        const { return m_Ptr == nullptr; }
		bool operator!=(std::nullptr_t)        const { return m_Ptr != nullptr; }

		void reset() { if (m_Ptr) { m_Ptr->Release(); m_Ptr = nullptr; } }

		template<typename U>
		IntrusiveRef<U> As() const { return IntrusiveRef<U>(dynamic_cast<U*>(m_Ptr)); }

	private:
		T* m_Ptr = nullptr;
	};

	// All Ref<T> are intrusive. T must derive from RefCounted.
	// IntrusiveRef<T> is valid even when T is incomplete, so Ref<T> works inside T's own class body.
	template<typename T>
	using Ref = IntrusiveRef<T>;

	template<typename T>
	using Scope = std::unique_ptr<T>;

	template<typename T>
	concept IsRefCounted = std::derived_from<T, RefCounted>;

	template<typename T, typename... Args>
	IntrusiveRef<T> CreateRef(Args&&... args) {
		return IntrusiveRef<T>(new T(std::forward<Args>(args)...));
	}

	template<typename T, typename... Args>
	constexpr Scope<T> CreateScope(Args&&... args) {
		return std::make_unique<T>(std::forward<Args>(args)...);
	}

}
