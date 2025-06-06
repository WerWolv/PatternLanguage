// construct_shared_object.hpp
//
// Written by Stephen Hewitt in 2025
// GitHub repo: https://github.com/shewitt-au/std-enable_shared_from_this-and-constructors

#pragma once
/*

The motivation for this code is issues with std::shared_ptr and
std::enable_shared_from_this.

Consider code like this:
  std::shared<SomeClass> ptr = std::make_shared<SomeClass>();

Assume that SomeClass's constructor uses shared_from_this. In this case, since
SomeClass is not yet owned by a std::shared_ptr, the shared_from_this call will
not work as expected. There seems to no way around this that I can find.
I wasn't aware of this issue. It was a real kick in the pants.

The solution implemented here is two-stage construction. First the object is
constructed and assigned to a std::shared_ptr; then, if present, a
post_construct method is called with the same arguments. All code that uses
shared_from_this in the constructors should be moved to a post_construct method.
The intent of the "if present" is to enable incremental migration to two-stage
construction on demand.

Another issue addressed is with std::make_shared. This function requires that
the constructor be public. If we're making factory methods for creation, we may
want to make the constructors non-public to stop misuse. In this case the code
uses std::shared_ptr's constructor and a new call. The function that calls new
can be made a friend of the class to grant it access. We lose the advantages of
using std::make_shared but gain protection against incorrect object
instantiation.
*/

#include <memory>
#include <utility>

namespace shared_object_creator {

// safe_enable_shared_from_this was found here:
//  https://stackoverflow.com/questions/8147027/how-do-i-call-stdmake-shared-on-a-class-with-only-protected-or-private-const
// 
// Posted by stackoverflow member Carsten.
// He adapted old code from stackoverflow member Zsolt Rizsányi,
// who in turn says it was invented by Jonathan Wakely (GCC developer).
// 
// The only modification I have made is to enable copy-construction.
// 
// C++23

class safe_enable_shared_from_this : public std::enable_shared_from_this<safe_enable_shared_from_this> {
    template<typename T, typename... Args>
        requires requires(T t, Args&&... args) {
          t.post_construct(std::forward<Args>(args)...);
    }
    friend std::shared_ptr<T> construct_shared_object(Args&&... args);

    template<typename T, typename... Args>
    friend std::shared_ptr<T> construct_shared_object(Args&&... args);
    
protected:
	safe_enable_shared_from_this() noexcept = default;
	safe_enable_shared_from_this(safe_enable_shared_from_this&&) noexcept = default;
	safe_enable_shared_from_this(const safe_enable_shared_from_this&) noexcept = default;
	safe_enable_shared_from_this& operator=(safe_enable_shared_from_this&&) noexcept = default;
	safe_enable_shared_from_this& operator=(const safe_enable_shared_from_this&) noexcept = delete;

public:
	virtual ~safe_enable_shared_from_this() noexcept = default;

protected:
    template <typename T>
    struct Allocator : public std::allocator<T>
    {  
        template<typename TParent, typename... TArgs>
        void construct(TParent* parent, TArgs&&... args)
        { ::new((void *)parent) TParent(std::forward<TArgs>(args)...); }
    };
    
//public:
    template <typename T, typename... TArgs>
    static inline auto create(TArgs&&... args) -> std::shared_ptr<T> {
        return std::allocate_shared<T>(Allocator<T>{}, std::forward<TArgs>(args)...);
    }

public:
	template <typename TSelf>
	auto inline shared_from_this(this TSelf&& self) noexcept
	{
		return std::static_pointer_cast<std::remove_reference_t<TSelf>>(
			std::forward<TSelf>(self).std::template enable_shared_from_this<safe_enable_shared_from_this>::shared_from_this());
	}
	
	template <typename TSelf>
	auto inline weak_from_this(this TSelf&& self) noexcept -> ::std::weak_ptr<std::remove_reference_t<TSelf>>
	{
		return std::static_pointer_cast<std::remove_reference_t<TSelf>>(
			std::forward<TSelf>(self).std::template enable_shared_from_this<safe_enable_shared_from_this>::weak_from_this().lock());
	}
};

// The actual creation functions.

template<typename T, typename... Args>
    requires requires(T t, Args&&... args) {
        t.post_construct(std::forward<Args>(args)...);
    }
std::shared_ptr<T> construct_shared_object(Args&&... args) {
    auto p = safe_enable_shared_from_this::create<T>(std::forward<Args>(args)...);
    p->post_construct(std::forward<Args>(args)...);
    return p;
}

template<typename T, typename... Args>
std::shared_ptr<T> construct_shared_object(Args&&... args) {
    return safe_enable_shared_from_this::create<T>(std::forward<Args>(args)...);
}

} // namespace shared_object_creator

#define BEFRIEND_CONSTRUCT_SHARED_OBJECT(T) \
    friend struct shared_object_creator::safe_enable_shared_from_this::Allocator<T>;
