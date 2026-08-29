#include "pt/util/arena.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <type_traits>
#include <vector>

namespace {

// Ownership is the whole point of Arena, so the fixtures count their own
// lifetimes. File-scope counters rather than members of a fixture object: the
// arena destroys its contents at scope exit, after any local observer is gone.
int live_nodes = 0;
int destroyed_nodes = 0;

class Node {
public:
    explicit Node(int id) noexcept : id_(id) { ++live_nodes; }

    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;

    // Virtual because Arena stores unique_ptr<Base>: deleting through the base
    // has to reach the derived destructor.
    virtual ~Node() {
        --live_nodes;
        ++destroyed_nodes;
    }

    [[nodiscard]] int id() const noexcept { return id_; }

private:
    int id_;
};

class Leaf final : public Node {
public:
    using Node::Node;
};

// Allocates its child from the arena it is itself being allocated into - the
// shape the engine builds trees in.
class Branch final : public Node {
public:
    Branch(pt::Arena<Node>& arena, int id) : Node(id), child_(arena.create<Leaf>(id + 1)) {}

    [[nodiscard]] const Node* child() const noexcept { return child_; }

private:
    const Node* child_;
};

// Neither derived from Node nor polymorphic: used only to check the constraints.
class Stranger {};

// Wrapping the template-id in a requires-expression makes an unsatisfied
// constraint a false answer instead of a compile error.
template <typename Base>
concept ArenaOver = requires { typename pt::Arena<Base>; };

} // namespace

TEST_CASE("create returns an observer of the object it built", "[util][arena]") {
    live_nodes = 0;
    pt::Arena<Node> arena;

    const Leaf* leaf = arena.create<Leaf>(7);

    REQUIRE(leaf != nullptr);
    REQUIRE(leaf->id() == 7);
    REQUIRE(live_nodes == 1);

    // The static type survives the call: callers store `const Sphere*`, not
    // `const Hittable*`, and would need a downcast if this decayed to the base.
    STATIC_REQUIRE((std::is_same_v<decltype(arena.create<Leaf>(0)), const Leaf*>));
}

TEST_CASE("addresses outlive the arena growing", "[util][arena]") {
    live_nodes = 0;
    pt::Arena<Node> arena;

    constexpr int count = 1000;
    std::vector<const Node*> observers;
    observers.reserve(count);

    for (int i = 0; i < count; ++i) {
        observers.push_back(arena.create<Leaf>(i));
    }

    // Enough creations to reallocate the internal vector many times. Only the
    // pointer array moves; the objects never do. Every non-owning
    // `const Hittable*` in the engine - HittableList, Bvh, Scene - depends on
    // exactly this, and none of them would survive a plain vector<T> arena.
    bool all_intact = true;
    for (int i = 0; i < count; ++i) {
        all_intact = all_intact && observers[static_cast<std::size_t>(i)]->id() == i;
    }

    REQUIRE(all_intact);
    REQUIRE(live_nodes == count);

    // Two creations are two objects, never a shared one.
    REQUIRE(observers.front() != observers.back());
}

TEST_CASE("the arena destroys everything it owns, through the base", "[util][arena]") {
    live_nodes = 0;
    destroyed_nodes = 0;

    {
        pt::Arena<Node> arena;
        for (int i = 0; i < 5; ++i) {
            static_cast<void>(arena.create<Leaf>(i));
        }
        REQUIRE(live_nodes == 5);
    }

    REQUIRE(live_nodes == 0);
    REQUIRE(destroyed_nodes == 5);
}

TEST_CASE("a constructor may allocate from the arena building it", "[util][arena]") {
    live_nodes = 0;
    pt::Arena<Node> arena;

    const Branch* branch = arena.create<Branch>(arena, 10);

    // The child was pushed during the parent's construction, so it sits in an
    // earlier slot than its parent. An implementation that reserved the parent's
    // slot first and filled it afterwards would have overwritten the child here,
    // leaving child_ dangling while every assertion on the parent still passed.
    REQUIRE(branch->id() == 10);
    REQUIRE(branch->child() != nullptr);
    REQUIRE(branch->child()->id() == 11);
    REQUIRE(live_nodes == 2);
}

TEST_CASE("the interface rejects what it cannot own", "[util][arena]") {
    // The base has to be polymorphic: the arena deletes through Base*.
    STATIC_REQUIRE(ArenaOver<Node>);
    STATIC_REQUIRE_FALSE(ArenaOver<Stranger>);
    STATIC_REQUIRE_FALSE(ArenaOver<int>);

    // derived_from and constructible_from, checked at the call site rather than
    // deep inside make_unique - a wrong type is a constraint failure, not a
    // page of template instantiation errors.
    STATIC_REQUIRE(requires(pt::Arena<Node>& a) { a.create<Leaf>(1); });
}
