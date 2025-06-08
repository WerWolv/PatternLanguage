// create_shared_object.hpp
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
want to make the constructors non-public to stop misuse. The solution to this
problem I can't take credit for. 

The code for enable_shared_from_nonpublic_constructor is based on code found here
(called safe_enable_shared_from_this):
    https://stackoverflow.com/questions/8147027/how-do-i-call-stdmake-shared-on-a-class-with-only-protected-or-private-const
Posted by stackoverflow member Carsten.
He adapted old code from stackoverflow member Zsolt Rizsányi,
who in turn says it was invented by Jonathan Wakely (GCC developer).

I have made minor modifications to suit and added comments.

Requires C++23
*/

#include <memory>
#include <utility>

// Used to enable create_shared_object to access non-public constructors.
#define BEFRIEND_create_shared_object(T) \
    friend struct shared_object_creator::enable_shared_from_nonpublic_constructor::Allocator<T>;

namespace shared_object_creator {

// enable_shared_from_nonpublic_constructor was found here:
//  https://stackoverflow.com/questions/8147027/how-do-i-call-stdmake-shared-on-a-class-with-only-protected-or-private-const
// 
// Posted by stackoverflow member Carsten.
// He adapted old code from stackoverflow member Zsolt Rizsányi,
// who in turn says it was invented by Jonathan Wakely (GCC developer).
class enable_shared_from_nonpublic_constructor : public std::enable_shared_from_this<enable_shared_from_nonpublic_constructor> {
    // Our friends. They're not from around here and have long hard to pronounce names.
    template<typename T, typename... Args>
        requires requires(T t, Args&&... args) {
          t.post_construct(std::forward<Args>(args)...);
    }
    friend std::shared_ptr<T> create_shared_object(Args&&... args);

    template<typename T, typename... Args>
    friend std::shared_ptr<T> create_shared_object(Args&&... args);

public:
	virtual ~enable_shared_from_nonpublic_constructor() noexcept = default;

    // The next two functions use C++23's "explicit object parameter"
    // syntax to make shared_from_this and weak_from_this, when called
    // from derived classes, return a std::shared_ptr<T> with T being
    // the derived type.
    template <typename TSelf>
	auto inline shared_from_this(this TSelf&& self) noexcept
	{
		return std::static_pointer_cast<std::remove_reference_t<TSelf>>(
			std::forward<TSelf>(self).std::template enable_shared_from_this<enable_shared_from_nonpublic_constructor>::shared_from_this());
	}
	
	template <typename TSelf>
	auto inline weak_from_this(this TSelf&& self) noexcept -> ::std::weak_ptr<std::remove_reference_t<TSelf>>
	{
		return std::static_pointer_cast<std::remove_reference_t<TSelf>>(
			std::forward<TSelf>(self).std::template enable_shared_from_this<enable_shared_from_nonpublic_constructor>::weak_from_this().lock());
	}

protected:
	enable_shared_from_nonpublic_constructor() noexcept = default;
	enable_shared_from_nonpublic_constructor(enable_shared_from_nonpublic_constructor&&) noexcept = default;
	enable_shared_from_nonpublic_constructor(const enable_shared_from_nonpublic_constructor&) noexcept = default;
	enable_shared_from_nonpublic_constructor& operator=(enable_shared_from_nonpublic_constructor&&) noexcept = default;
	enable_shared_from_nonpublic_constructor& operator=(const enable_shared_from_nonpublic_constructor&) noexcept = delete;

    template <typename T>
    struct Allocator : public std::allocator<T>
    {  
        template<typename TParent, typename... TArgs>
        void construct(TParent* parent, TArgs&&... args)
        { ::new((void *)parent) TParent(std::forward<TArgs>(args)...); }
    };
    
private:
    // std::allocate_share, like std::make_shared, allocates both the object
    // and the control block using only a single allocation.
    template <typename T, typename... TArgs>
    static inline auto create(TArgs&&... args) -> std::shared_ptr<T> {
        return std::allocate_shared<T>(Allocator<T>{}, std::forward<TArgs>(args)...);
    }
};

// The actual creation functions.

template<typename T, typename... Args>
    requires requires(T t, Args&&... args) {
        t.post_construct(std::forward<Args>(args)...);
    }
std::shared_ptr<T> create_shared_object(Args&&... args) {
    auto p = enable_shared_from_nonpublic_constructor::create<T>(std::forward<Args>(args)...);
    p->post_construct(std::forward<Args>(args)...);
    return p;
}

template<typename T, typename... Args>
std::shared_ptr<T> create_shared_object(Args&&... args) {
    return enable_shared_from_nonpublic_constructor::create<T>(std::forward<Args>(args)...);
}
 
} // namespace shared_object_creator
