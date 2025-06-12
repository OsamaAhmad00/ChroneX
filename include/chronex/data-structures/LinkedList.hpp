#pragma once

#include <memory>
#include <algorithm>
#include <cassert>
#include <type_traits>

namespace chronex::ds {

template <typename T>
struct LinkedListNode {

    constexpr LinkedListNode() noexcept requires(std::is_default_constructible_v<T>) = default;

    template <typename... Args>
    constexpr LinkedListNode(LinkedListNode* prev = nullptr, LinkedListNode* next = nullptr, Args&&... args)
        : _prev(prev), _next(next), _data(std::forward<Args>(args)...) {}

    T&       data()       noexcept { return _data; }
    const T& data() const noexcept { return _data; }

    LinkedListNode*       prev()        { return _prev; }
    const LinkedListNode* prev() const  { return _prev; }
    LinkedListNode*       next()        { return _next; }
    const LinkedListNode* next() const  { return _next; }
    void set_prev(LinkedListNode* prev) { _prev = prev; }
    void set_next(LinkedListNode* next) { _next = next; }

private:

    LinkedListNode* _prev = nullptr;
    LinkedListNode* _next = nullptr;
    T _data;
};


template <typename T, template <typename> class Allocator = std::allocator>
class LinkedList : private Allocator<LinkedListNode<T>> {

    using Node = LinkedListNode<T>;

    using AllocTraits = std::allocator_traits<Allocator<Node>>;

    constexpr static auto prev_func = [] (auto* node) { assert(node != nullptr && "Node is nullptr!"); return node->prev(); };
    constexpr static auto next_func = [] (auto* node) { assert(node != nullptr && "Node is nullptr!"); return node->next(); };

    using PrevFunc = decltype(prev_func);
    using NextFunc = decltype(next_func);

    template <typename NodeType, typename NextFunc, typename PrevFunc>
    class Iterator {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = std::conditional_t<std::is_const_v<NodeType>, const T*, T*>;
        using reference = std::conditional_t<std::is_const_v<NodeType>, const T&, T&>;

        constexpr explicit Iterator() noexcept : _node(nullptr) { }

        constexpr explicit Iterator(NodeType* node) noexcept : _node(node) { }

        template <typename OtherNode, typename OtherNext, typename OtherPrev>
        explicit constexpr Iterator(const Iterator<OtherNode, OtherNext, OtherPrev>& it) noexcept
            : _node(it.node) { }

        constexpr bool is_null() const noexcept { return _node == nullptr; };

        constexpr reference operator* () const noexcept { return  _node->data(); }
        constexpr pointer   operator->() const noexcept { return &_node->data(); }

        [[nodiscard]] constexpr bool is_invalidated() const noexcept { return _node == nullptr; }

        constexpr Iterator& operator++() noexcept {
            _node = NextFunc { } (_node);
            return *this;
        }

        constexpr Iterator operator++(int) noexcept {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        constexpr Iterator& operator--() noexcept {
            _node = PrevFunc { } (_node);
            return *this;
        }

        constexpr Iterator operator--(int) noexcept {
            Iterator tmp = *this;
            --(*this);
            return tmp;
        }

        friend constexpr bool operator==(const Iterator& a, const Iterator& b) noexcept {
            return a.node() == b.node();
        }

        friend constexpr bool operator!=(const Iterator& a, const Iterator& b) noexcept {
            return !(a == b);
        }

        // Make all iterator types friends to access the node pointer
        template <typename OtherNode, typename OtherNext, typename OtherPrev>
        friend class Iterator;

        void invalidate() {
            _node = nullptr;
        }

    private:

        friend LinkedList;

        constexpr auto node() const noexcept { return _node; }

        NodeType* _node;
    };

public:

    using value_type = T;
    using iterator = Iterator<Node, NextFunc, PrevFunc>;
    using const_iterator = Iterator<const Node, NextFunc, PrevFunc>;
    using reverse_iterator = Iterator<Node, PrevFunc, NextFunc>;
    using const_reverse_iterator = Iterator<const Node, PrevFunc, NextFunc>;

    template <typename Iter>
    constexpr void link_node(Iter pos, Node* new_node) noexcept {
        new_node->set_prev(pos.node()->prev());
        new_node->set_next(pos.node());
        auto node = pos.node();
        node->prev()->set_next(new_node);
        node->set_prev(new_node);
        ++_size;
    }

    template <typename Node>
    constexpr void link_node_front(Node new_node) noexcept {
        return link_node(begin(), new_node);
    }

    template <typename Node>
    constexpr void link_node_back(Node new_node) noexcept {
        return link_node(end(), new_node);
    }

    constexpr void unlink_node(Node* node) noexcept {
        node->next()->set_prev(node->prev());
        node->prev()->set_next(node->next());
        // Don't need to set the pointer to nullptr
        // node->set_prev(nullptr);
        // node->set_next(nullptr);
        --_size;
    }

    template <typename Iter>
    constexpr void link_node(Iter pos, iterator new_node) noexcept {
        return link_node(pos, new_node.node());
    }

    constexpr void unlink_node(iterator new_node) noexcept {
        return unlink_node(new_node.node());
    }

    constexpr void copy_back(const LinkedList& other) requires (std::is_copy_constructible_v<T>) {
        for (auto it = other.begin(); it != other.end(); ++it) {
            push_back(*it);
        }
    }

    constexpr void move(LinkedList&& other) noexcept {
        if (other.empty()) {
            dummy_tail().set_prev(&dummy_head());
            dummy_head().set_next(&dummy_tail());
            _size = 0;
            return;
        }

        dummy_tail().set_prev(other.dummy_tail().prev());
        dummy_tail().prev()->set_next(&dummy_tail());
        dummy_head().set_next(other.dummy_head().next());
        dummy_head().next()->set_prev(&dummy_head());
        _size = other._size;

        other.dummy_tail().set_prev(&other.dummy_head());
        other.dummy_head().set_next(&other.dummy_tail());
        other._size = 0;
    }

    constexpr LinkedList() noexcept { dummy_head().set_next(&dummy_tail()); dummy_tail().set_prev(&dummy_head()); }

    constexpr LinkedList(Node* head, Node* tail, size_t size) noexcept : _size(size) {
        dummy_head().set_next(head);
        head->set_prev(&dummy_head());

        dummy_tail().set_prev(tail);
        tail->set_next(&dummy_tail());
    }

    constexpr LinkedList(const LinkedList& other) requires (std::is_copy_constructible_v<T>) : LinkedList() { copy_back(other); }

    constexpr LinkedList(LinkedList&& other) noexcept : LinkedList() { move(std::move(other)); }

    constexpr explicit LinkedList(size_t n, const T& value = T()) : LinkedList() {
        for (size_t i = 0; i < n; ++i) push_back(value);
    }

    constexpr LinkedList& operator=(const LinkedList& other) noexcept {
        LinkedList tmp(other);
        std::swap(*this, tmp);
        return *this;
    }

    constexpr LinkedList& operator=(LinkedList&& other) noexcept {
        clear();
        move(std::move(other));
        return *this;
    }

    ~LinkedList() noexcept { clear(); }

    constexpr iterator begin() noexcept { return iterator { dummy_head().next() }; }
    constexpr iterator end() noexcept { return iterator { &dummy_tail() } ; }
    constexpr const_iterator begin() const noexcept { return const_iterator { dummy_head().next() }; }
    constexpr const_iterator end() const noexcept { return const_iterator { &dummy_tail() }; }

    constexpr const_iterator cbegin() const noexcept { return begin(); }
    constexpr const_iterator cend() const noexcept { return end(); }

    constexpr reverse_iterator rbegin() noexcept { return reverse_iterator { dummy_tail().prev() }; }
    constexpr reverse_iterator rend() noexcept { return reverse_iterator { &dummy_head() }; }
    constexpr const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator { dummy_head().next() }; }
    constexpr const_reverse_iterator rend() const noexcept { return const_reverse_iterator { &dummy_tail() }; }

    constexpr const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator { dummy_tail().prev() }; }
    constexpr const_reverse_iterator crend() const noexcept { return const_reverse_iterator { &dummy_head() }; }

    constexpr size_t size() const noexcept { return _size; }
    constexpr bool empty() const noexcept { return _size == 0; }

    template <typename Self>
    constexpr auto& front(this Self&& self) noexcept {
        assert(!self.empty() && "List is empty");
        return *self.begin();
    }

    template <typename Self>
    constexpr auto& back(this Self&& self) noexcept {
        assert(!self.empty() && "List is empty");
        return *self.rbegin();
    }

    template <typename Iter, typename... Args>
    constexpr Iter insert(Iter pos, Args&&... args) {
        return emplace(pos, std::forward<Args>(args)...);
    }

    template <typename U>
    constexpr void push_front(U&& value) { insert(begin(), std::forward<U>(value)); }

    template <typename... Args>
    constexpr iterator emplace_front(Args&&... args) {
        return emplace(begin(), std::forward<Args>(args)...);
    }

    template <typename... Args>
    constexpr iterator emplace_back(Args&&... args) {
        return emplace(end(), std::forward<Args>(args)...);
    }

    template <typename U>
    constexpr void push_back(U&& value) { insert(end(), std::forward<U>(value)); }

    constexpr void pop_front() noexcept { erase(begin()); }

    constexpr void pop_back() noexcept { erase(std::prev(end())); }

    template <typename Iter, typename... Args>
    constexpr Iter emplace(Iter pos, Args&&... args) {
        struct Deleter {
            Allocator<Node>& allocator;
            void operator()(Node* node) {
                AllocTraits::deallocate(allocator, node, 1);
            }
        };
        auto pos_node = pos.node();
        auto node = std::unique_ptr<Node, Deleter> { AllocTraits::allocate(*this, 1), Deleter { *this } };
        // If construction fails, we need to still release the allocated memory
        AllocTraits::construct(*this, node.get(), pos_node->prev(), pos_node, std::forward<Args>(args)...);
        pos_node->prev()->set_next(node.get());
        pos_node->set_prev(node.get());
        ++_size;
        return Iter { node.release() };
    }

    // TODO change this design so that it complies more with other containers
    template <typename Iter>
    constexpr void free(Iter pos) noexcept {
        Node* node = pos.node();
        AllocTraits::destroy(*this, node);
        AllocTraits::deallocate(*this, node, 1);
    }

    template <typename Iter>
    constexpr iterator erase(Iter pos) noexcept {
        assert(pos.node() != &dummy_head() && pos.node() != &dummy_tail() && "Cannot erase at the end() or rend() iterator");
        Node* node = pos.node();
        Node* next = node->next();
        unlink_node(node);
        free(pos);
        pos.invalidate();
        return Iter { next };
    }

    template <typename Iter>
    constexpr iterator erase(Iter first, Iter last) noexcept {
        while (first != last) first = erase(first);
        return iterator { first.node() };
    }

    template <typename Iter>
    constexpr iterator extract(Iter pos) noexcept {  // Returning a normal iterator on purpose
        assert(pos.node() != &dummy_head() && pos.node() != &dummy_tail() && "Cannot extract end() or rend()");
        unlink_node(pos.node());
        return iterator { pos };
    }

    template <typename Iter>
    constexpr LinkedList extract_range(Iter begin, Iter end) noexcept {
        if (begin == end) return { };

        Node* first = begin.node();
        Node* last = end.node()->prev();
        assert(first != &dummy_head() && first != &dummy_tail() && "Cannot extract end() or rend()");

        size_t count = std::distance(begin, end);
        _size -= count;

        Node* before = first->prev();
        Node* after = end.node();
        before->set_next(after);
        after->set_prev(before);

        return LinkedList { first, last, count };
    }

    template <typename Iter>
    constexpr Iter splice(Iter pos, LinkedList& other, Iter begin, Iter end) noexcept {
        if (begin == end || pos == other.end()) return pos;

        assert(begin.node() != &dummy_head() && begin.node() != &dummy_tail() && "Cannot splice end() or rend()");

        size_t count = std::distance(begin, end);

        Node* first = begin.node();
        Node* last = end.node()->prev();  // Last included

        // Unlink from the other list
        Node* before = first->prev();
        Node* after = end.node();
        before->set_next(after);
        after->set_prev(before);
        other._size -= count;

        // Link into this list
        Node* pos_node = pos.node();
        first->set_prev(pos_node->prev());
        pos_node->prev()->set_next(first);
        last->set_next(pos_node);
        pos_node->set_prev(last);
        _size += count;

        return Iter { first };
    }

    constexpr void clear() noexcept {
        erase(begin(), end());
    }

private:

    // Written this way to avoid errors about trying to
    //  default-construct type of T that are not default
    //  constructable
    alignas(alignof(Node)) char _dummy_head[sizeof(Node)];
    alignas(alignof(Node)) char _dummy_tail[sizeof(Node)];

    Node& dummy_head() { return *reinterpret_cast<Node*>(&_dummy_head); }
    Node& dummy_tail() { return *reinterpret_cast<Node*>(&_dummy_tail); }
    const Node& dummy_head() const { return *reinterpret_cast<const Node*>(&_dummy_head); }
    const Node& dummy_tail() const { return *reinterpret_cast<const Node*>(&_dummy_tail); }

    size_t _size = 0;
};

}