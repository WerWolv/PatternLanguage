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

Assume that `SomeClass`'s constructor uses shared_from_this. In this case, since
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

#include <iostream>
#include <concepts>
#include <memory>

#define BEFRIEND_SHARED_OBJECT_CREATOR \
 template <typename T, typename... Args> \
 friend std::shared_ptr<T> shared_object_creator::impl::shared_ptr_creator(Args&&... args);

namespace shared_object_creator {

    namespace impl {

        /*
        shared_ptr_creator is responsible for actually creating the std::shared_ptr.
        As described in this header's opening comment, it decides whether to use
        std::make_shared or std::shared_ptr's constructor and new based on the
        accessibility the the class' constructor. If you wish to make the constructors
        non-public make this a friend. The BEFRIEND_SHARED_OBJECT_CREATOR macro can be
        used to do this tersely.
        */

        template <typename T, typename... Args>
            requires std::constructible_from<T, Args...>
        std::shared_ptr<T> shared_ptr_creator(Args&&... args) {
            return std::make_shared<T>(std::forward<Args>(args)...);
        }

        template <typename T, typename... Args>
        std::shared_ptr<T> shared_ptr_creator(Args&&... args) {
            return std::shared_ptr<T>(new T(std::forward<Args>(args)...));
        }

        // Used to detect if the class has a post_construct method with the same
        // arguments as the constructor.
        template<typename T, typename... Args>
        concept HasPostConstruct = requires(T obj, Args... args) {
            obj.post_construct(args...);
        };

    } // namespace impl

    // The actual creation functions.

    template<typename T, typename... Args>
    std::shared_ptr<T> construct_shared_object(Args&&... args) {
        return impl::shared_ptr_creator<T>(std::forward<Args>(args)...);
    }

    template <impl::HasPostConstruct T, typename... Args>
    std::shared_ptr<T> construct_shared_object(Args&&... args) {
        std::shared_ptr<T> p = impl::shared_ptr_creator<T>(std::forward<Args>(args)...);
        p->post_construct(std::forward<Args>(args)...);
        return p;
    }

} // namespace shared_object_creator
