#pragma once

#include <cassert>
#include <iostream>
#include <unordered_map>
#include <vector>

template <typename Key, typename Value, typename Hash = std::hash<Key>,
          typename Equal = std::equal_to<Key>,
          typename MapAlloc = std::allocator<std::pair<const Key, Value>>>
class UnorderedMap {
  private:
    template <typename T, typename Alloc = std::allocator<T>>
    class List {
      private:
        struct BaseNode {
            BaseNode* prev;
            BaseNode* next;
            BaseNode()
                : prev(this), next(this) {}
            BaseNode(BaseNode* pr, BaseNode* nxt)
                : prev(pr), next(nxt) {
                pr->next = this;
                nxt->prev = this;
            }
            ~BaseNode() {}
        };
        struct Node : BaseNode {
            T* value;
            Node() = default;
            Node(T* ptr_val, BaseNode* pr, BaseNode* nxt)
                : BaseNode(pr, nxt), value(ptr_val) {}
            ~Node() {}
        };

      public:
        friend UnorderedMap;
        using NodeAlloc =
            typename std::allocator_traits<Alloc>::template rebind_alloc<Node>;
        using NodeTraits = std::allocator_traits<NodeAlloc>;
        using AllocTraits = std::allocator_traits<Alloc>;
        template <typename U>
        class BasicIterator {
          private:
            BaseNode* node_;

          public:
            friend List;
            friend UnorderedMap;
            using value_type = U;
            using difference_type = std::ptrdiff_t;
            using pointer = U*;
            using reference = U&;
            using iterator_category = std::bidirectional_iterator_tag;

            explicit BasicIterator()
                : node_(nullptr) {}
            BasicIterator(const BasicIterator& copy)
                : node_(copy.node_) {}
            BasicIterator(const BaseNode* node)
                : node_(node->next->prev) {}
            BasicIterator& operator=(const BasicIterator& copy) {
                if (this != &copy) {
                    node_ = copy.node_;
                }
                return *this;
            }

            BasicIterator& operator++() {
                node_ = node_->next;
                return *this;
            }

            BasicIterator operator++(int) {
                BasicIterator copy = *this;
                ++(*this);
                return copy;
            }

            BasicIterator& operator--() {
                node_ = node_->prev;
                return *this;
            }

            BasicIterator operator--(int) {
                BasicIterator copy = *this;
                --(*this);
                return copy;
            }

            reference operator*() {
                Node* tmp = static_cast<Node*>(node_);
                return static_cast<U&>(*(tmp->value));
            }

            pointer operator->() {
                return static_cast<Node*>(node_)->value;
            }

            pointer operator->() const {
                return static_cast<const Node*>(node_);
            }

            bool operator==(const BasicIterator& other) const {
                return node_ == other.node_;
            }

            operator BasicIterator<const T>() const {
                return *(reinterpret_cast<const BasicIterator<const T>*>(this));
            }
        };

        using iterator = BasicIterator<T>;
        using const_iterator = BasicIterator<const T>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;
        using reverse_iterator = std::reverse_iterator<iterator>;

      private:
        [[no_unique_address]] Alloc alloc_;
        [[no_unique_address]] NodeAlloc nodalloc_;  //[[no_unique_address]]

        size_t size_ = 0;
        BaseNode fakeNode_;

        void deleteNode(BaseNode* ptr) {
            Node* node_ptr = static_cast<Node*>(ptr);
            T* dataPtr = node_ptr->value;
            BaseNode* prev = ptr->prev;
            BaseNode* next = ptr->next;
            AllocTraits::destroy(alloc_, dataPtr);
            AllocTraits::deallocate(alloc_, dataPtr, 1);
            NodeTraits::destroy(nodalloc_, node_ptr);
            NodeTraits::deallocate(nodalloc_, node_ptr, 1);
            prev->next = next;
            next->prev = prev;
            --size_;
        }

        void deleteShell(BaseNode* ptr) {
            Node* node_ptr = static_cast<Node*>(ptr);
            BaseNode* prev = ptr->prev;
            BaseNode* next = ptr->next;
            NodeTraits::destroy(nodalloc_, node_ptr);
            NodeTraits::deallocate(nodalloc_, node_ptr, 1);
            prev->next = next;
            next->prev = prev;
            --size_;
        }

        template <typename... Args>
        BaseNode* emplace(BaseNode* prev, BaseNode* next, Args&&... args) {
            Node* newnode = nullptr;
            T* newData = nullptr;
            newData = AllocTraits::allocate(alloc_, 1);
            try {
                AllocTraits::construct(alloc_, newData, std::forward<Args>(args)...);
            } catch (...) {
                AllocTraits::deallocate(alloc_, newData, 1);
                throw;
            }
            newnode = NodeTraits::allocate(nodalloc_, 1);
            try {
                NodeTraits::construct(nodalloc_, newnode, newData, prev, next);
            } catch (...) {
                AllocTraits::destroy(alloc_, newData);
                AllocTraits::deallocate(alloc_, newData, 1);
                NodeTraits::deallocate(nodalloc_, newnode, 1);
                throw;
            }
            ++size_;
            return static_cast<BaseNode*>(newnode);
        }

        BaseNode* push_ptr(BaseNode* prev, BaseNode* next, T* newData) {
            Node* newnode = nullptr;
            newnode = NodeTraits::allocate(nodalloc_, 1);
            try {
                NodeTraits::construct(nodalloc_, newnode, newData, prev, next);
            } catch (...) {
                NodeTraits::deallocate(nodalloc_, newnode, 1);
                throw;
            }
            ++size_;
            return static_cast<BaseNode*>(newnode);
        }

        BaseNode* push_ptr(const_iterator it, T* newData) {
            return push_ptr(it.node_->prev, it.node_, newData);
        }

        void destroyAll() {
            while (size_ != 0) {
                pop_back();
            }
        }

      public:
        List() {}

        explicit List(const Alloc& alloc)
            : alloc_(alloc), nodalloc_(alloc_) {}

        explicit List(size_t count, const T& value, const Alloc& alloc = Alloc())
            : alloc_(alloc), nodalloc_(alloc) {
            for (size_t i = 0; i < count; ++i) {
                try {
                    push_back(value);
                } catch (...) {
                    destroyAll();
                    throw;
                }
            }
        }

        explicit List(size_t count, const Alloc& alloc = Alloc())
            : alloc_(alloc), nodalloc_(alloc_) {
            for (size_t i = 0; i < count; ++i) {
                try {
                    emplace(fakeNode_.prev, &fakeNode_);
                } catch (...) {
                    destroyAll();
                    throw;
                }
            }
        }

        List(const List& other)
            : alloc_(
                  AllocTraits::select_on_container_copy_construction(other.alloc_)),
              nodalloc_(alloc_) {
            size_ = 0;
            if (other.size_ != 0) {
                BaseNode* curr = other.fakeNode_.next;
                for (size_t i = 0; i < other.size_; ++i) {
                    T* data = static_cast<Node*>(curr)->value;
                    try {
                        push_back(*data);
                    } catch (...) {
                        destroyAll();
                        throw;
                    }
                    curr = curr->next;
                }
            }
        }

        List(List&& other)
            : alloc_(std::move(NodeTraits::select_on_container_copy_construction(
                  other.alloc_))) {
            std::swap(fakeNode_, other.fakeNode_);
            std::swap(size_, other.size_);
        }

        List& operator=(const List& other) {
            if (this == &other) {
                return *this;
            }
            List tmp;
            if (AllocTraits::propagate_on_container_copy_assignment::value) {
                tmp.alloc_ = other.alloc_;
                tmp.nodalloc_ = other.nodalloc_;
            } else {
                tmp.alloc_ = alloc_;
                tmp.nodalloc_ = nodalloc_;
            }
            if (other.size_ != 0) {
                BaseNode* curr = other.fakeNode_.next;
                for (size_t i = 0; i < other.size_; ++i) {
                    T* data = static_cast<Node*>(curr)->value;
                    try {
                        tmp.push_back(*data);
                    } catch (...) {
                        tmp.destroyAll();
                        return *this;
                    }
                    curr = curr->next;
                }
            }
            destroyAll();
            std::swap(fakeNode_, tmp.fakeNode_);
            std::swap(size_, tmp.size_);
            std::swap(alloc_, tmp.alloc_);
            std::swap(nodalloc_, tmp.nodalloc_);
            return *this;
        }

        void swap_fakes(BaseNode& other) {
            std::swap(fakeNode_, other);
        }

        List& operator=(List&& other) {
            if (this != &other) {
                if (AllocTraits::propagate_on_container_copy_assignment::value) {
                    alloc_ = other.alloc_;
                }
                if (NodeTraits::propagate_on_container_copy_assignment::value) {
                    nodalloc_ = other.nodalloc_;
                }
                destroyAll();
                if (other.size_ == 0) {
                    std::swap(fakeNode_, other.fakeNode_);
                    std::swap(fakeNode_.next, other.fakeNode_.next);
                    std::swap(fakeNode_.prev, other.fakeNode_.prev);
                } else {
                    fakeNode_.next = other.fakeNode_.next;
                    fakeNode_.prev = other.fakeNode_.prev;
                    other.fakeNode_.next->prev = &fakeNode_;
                    other.fakeNode_.prev->next = &fakeNode_;
                    other.fakeNode_.next = &other.fakeNode_;
                    other.fakeNode_.prev = &other.fakeNode_;
                }
                std::swap(size_, other.size_);
            }

            return *this;
        }

        NodeAlloc& get_allocator() {
            return nodalloc_;
        }
        NodeAlloc& get_allocator() const {
            return nodalloc_;
        }

        size_t size() const {
            return size_;
        }

        void push_back(const T& value) {
            emplace(fakeNode_.prev, &fakeNode_, value);
        }

        void push_back(T&& value) {
            emplace(fakeNode_.prev, &fakeNode_, std::move(value));
        }

        void push_back(T* ptr_value) {
            push_ptr(fakeNode_.prev, &fakeNode_, ptr_value);
        }

        void push_front(const T& value) {
            emplace(&fakeNode_, fakeNode_.next, value);
        }

        void push_front(T* ptr_value) {
            push_ptr(&fakeNode_, fakeNode_.next, ptr_value);
        }

        void push_front(T&& value) {
            emplace(&fakeNode_, fakeNode_.next, std::move(value));
        }

        void pop_back() {
            deleteNode(fakeNode_.prev);
        }

        void pop_front() {
            deleteNode(fakeNode_.next);
        }

        iterator begin() {
            return iterator(fakeNode_.next);
        }

        iterator end() {
            return iterator(&fakeNode_);
        }

        const_iterator begin() const {
            return const_iterator(fakeNode_.next);
        }

        const_iterator end() const {
            return const_iterator(&fakeNode_);
        }

        const_iterator cbegin() const {
            return const_iterator(fakeNode_.next);
        }
        const_iterator cend() const {
            return const_iterator(&fakeNode_);
        }

        std::reverse_iterator<iterator> rbegin() {
            return std::reverse_iterator<iterator>(iterator(&fakeNode_));
        }

        const_reverse_iterator rbegin() const {
            return const_reverse_iterator(cend());
        }

        const_reverse_iterator rend() const {
            return const_reverse_iterator(cbegin());
        }

        std::reverse_iterator<iterator> rend() {
            return std::reverse_iterator<iterator>(iterator(fakeNode_.next));
        }

        void insert(const_iterator iter, const T& el) {
            emplace(iter.node_->prev, iter.node_, el);
        }

        void insert(const_iterator iter, T&& el) {
            emplace(iter.node_->prev, iter.node_, std::move(el));
        }

        void erase(const_iterator iter) {
            deleteNode(iter.node_);
        }

        ~List() {
            destroyAll();
        }
    };

  public:
    using NodeType = std::pair<const Key, Value>;

  private:
    using BaseNodePtr = typename List<NodeType, MapAlloc>::BaseNode*;
    using DataNodePtr = typename List<NodeType, MapAlloc>::Node*;
    size_t table_size_ = 128;
    List<NodeType, MapAlloc> inner_list_;
    std::vector<BaseNodePtr> table_;
    Hash hash_ = Hash();
    Equal equal_ = Equal();
    MapAlloc alloc_ = MapAlloc();
    double load_factor_ = 0;
    double max_load_factor_ = 0.8;

  public:
    using iterator = typename List<NodeType, MapAlloc>::iterator;
    using const_iterator = typename List<NodeType, MapAlloc>::const_iterator;
    using AllocTraits = std::allocator_traits<MapAlloc>;

    iterator begin() {
        return inner_list_.begin();
    }
    iterator end() {
        return inner_list_.end();
    }
    const_iterator begin() const {
        return inner_list_.begin();
    }
    const_iterator end() const {
        return inner_list_.end();
    }

    const_iterator cbegin() const {
        return inner_list_.cbegin();
    }
    const_iterator cend() const {
        return inner_list_.cend();
    }
    UnorderedMap()
        : inner_list_(), table_(table_size_, nullptr) {}
    UnorderedMap(const UnorderedMap& copy)
        : table_(table_size_, nullptr),
          alloc_(
              AllocTraits::select_on_container_copy_construction(copy.alloc_)) {
        insert(copy.begin(), copy.end());
    }
    UnorderedMap(UnorderedMap&& other)
        : inner_list_(std::move(other.inner_list_)),
          table_(std::move(other.table_)),
          hash_(std::move(other.hash_)),
          equal_(std::move(other.equal_)),
          alloc_(AllocTraits::select_on_container_copy_construction(
              std::move(other.alloc_))) {
        other.load_factor_ = 0;
        max_load_factor_ = other.max_load_factor_;
    }

    UnorderedMap& operator=(const UnorderedMap& other) {
        MapAlloc all = alloc_;
        if (AllocTraits::propagate_on_container_copy_assignment::value) {
            all = other.alloc_;
        }
        UnorderedMap temp(all);
        for (auto it = other.begin(); it != other.end(); ++it) {
            try {
                temp.insert(*it);
            } catch (...) {
                temp.inner_list_.destroyAll();
                return *this;
            }
        }
        std::swap(inner_list_.fakeNode_, temp.inner_list_.fakeNode_);
        std::swap(inner_list_.size_, temp.inner_list_.size_);
        std::swap(table_, temp.table_);
        std::swap(alloc_, temp.alloc_);
        std::swap(load_factor_, temp.load_factor_);
        std::swap(max_load_factor_, temp.max_load_factor_);
        std::swap(table_size_, temp.table_size_);
        return *this;
    }

    UnorderedMap& operator=(UnorderedMap&& other) {
        if (this != &other) {
            if (AllocTraits::propagate_on_container_move_assignment::value) {
                alloc_ = std::move(other.alloc_);
            }
            // inner_list_.destroyAll();
            max_load_factor_ = other.max_load_factor_;
            load_factor_ = std::move(other.load_factor_);
            table_ = std::move(other.table_);
            hash_ = std::move(other.hash_);
            equal_ = std::move(other.equal_);
            inner_list_ = std::move(other.inner_list_);
            table_size_ = std::move(other.table_size_);
            other.table_size_ = 128;
        }
        return *this;
    }
    ~UnorderedMap() {}

    iterator find(const Key& key) {
        size_t hash = hash_(key) % table_size_;
        if (table_[hash] == nullptr) {
            return inner_list_.end();
        }
        BaseNodePtr start = table_[hash]->next;
        iterator it(start);
        while (it != inner_list_.end()) {
            if (hash_(it->first) % table_size_ != hash) {
                return inner_list_.end();
            }
            if (equal_(it->first, key)) {
                return it;
            }
            ++it;
        }
        return inner_list_.end();
    }

    iterator find(Key&& key) {
        size_t hash = hash_(key) % table_size_;
        if (table_[hash] == nullptr) {
            return inner_list_.end();
        }
        BaseNodePtr start = table_[hash]->next;
        iterator it(start);

        while (it != inner_list_.end()) {
            if (hash_(it->first) % table_size_ != hash) {
                return inner_list_.end();
            }
            if (equal_(it->first, key)) {
                return it;
            }
            ++it;
        }
        return inner_list_.end();
    }

    const_iterator find(const Key& key) const {
        return static_cast<const_iterator>(find(key));
    }

    const_iterator find(Key&& key) const {
        return static_cast<const_iterator>(find(std::move(key)));
    }

    void print() {
        inner_list_.print();
    }

    void rehash(size_t sz) {
        auto temp = List<NodeType, MapAlloc>(alloc_);
        table_size_ = sz;
        table_ = std::vector<BaseNodePtr>(table_size_ * 2, nullptr);
        while (begin() != end()) {
            auto it = begin();
            const Key& key = it->first;
            size_t obj_hash = hash_(key) % table_size_;
            if (table_[obj_hash] != nullptr) {
                temp.push_ptr(table_[obj_hash], table_[obj_hash]->next, &(*it));
            } else {
                table_[obj_hash] = temp.fakeNode_.prev;
                temp.push_back(&(*it));
            }
            inner_list_.deleteShell(it.node_);
        }
        inner_list_ = std::move(temp);
        if (size() > 0) {
            size_t hs = hash_(static_cast<DataNodePtr>(inner_list_.fakeNode_.next)
                                  ->value->first) %
                        table_size_;
            table_[hs] = &inner_list_.fakeNode_;
        }
    }

    void reserve(size_t count) {
        if (count / static_cast<double>(table_size_) >= max_load_factor_) {
            rehash(2 * count / max_load_factor_);
        }
    }

    void max_load_factor(double max_load) {
        max_load_factor_ = max_load;
    }

    double load_factor() const {
        return load_factor_;
    }

    double max_load_factor() const {
        return max_load_factor_;
    }

    template <typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args) {
        NodeType* newNodePtr = AllocTraits::allocate(alloc_, 1);
        try {
            AllocTraits::construct(alloc_, newNodePtr, std::forward<Args>(args)...);
        } catch (...) {
            AllocTraits::deallocate(alloc_, newNodePtr, 1);
            throw;
        }
        const Key& key = newNodePtr->first;
        size_t obj_hash = hash_(key) % table_size_;
        iterator it = find(key);
        bool found = true;
        if (it != end()) {
            erase(it);
            found = false;
        }
        if (load_factor_ >= max_load_factor_) {
            try {
                rehash(table_size_ * 2);
            } catch (...) {
                AllocTraits::destroy(alloc_, newNodePtr);
                AllocTraits::deallocate(alloc_, newNodePtr, 1);
                throw;
            }
        }
        load_factor_ = static_cast<double>(inner_list_.size()) / table_.size();
        if (table_[obj_hash] == nullptr) {
            table_[obj_hash] = inner_list_.fakeNode_.prev;
            inner_list_.push_ptr(inner_list_.fakeNode_.prev, &inner_list_.fakeNode_,
                                 newNodePtr);
            inner_list_.begin();
            return std::make_pair(--end(), found);
        }
        BaseNodePtr prev = table_[obj_hash];
        BaseNodePtr next = table_[obj_hash]->next;
        inner_list_.push_ptr(prev, next, newNodePtr);
        return {iterator(prev->next), found};
    }

    std::pair<iterator, bool> insert(NodeType&& newnode) {
        return emplace(std::move(newnode));
    }

    std::pair<iterator, bool> insert(const NodeType& newnode) {
        return emplace(newnode);
    }

    template <typename P>
    std::pair<iterator, bool> insert(P&& value) {
        return emplace(std::forward<P>(value));
    }

    template <typename InputIterator>
    void insert(const InputIterator& it_start, const InputIterator& it_end) {
        for (auto curr_it = it_start; curr_it != it_end; ++curr_it) {
            insert(*curr_it);
        }
    }

    void erase(iterator it) {
        BaseNodePtr ptr = it.node_;
        BaseNodePtr next = ptr->next;
        size_t hs = hash_(it->first) % table_size_;
        BaseNodePtr prev = table_[hs];
        if (ptr == table_[hs]->next) {
            table_[hs] = nullptr;
        }
        inner_list_.erase(it);
        if (next != &inner_list_.fakeNode_) {
            hs = hash_(iterator(next)->first) % table_size_;
            table_[hs] = prev;
        }
        if (size() == 0) {
        }
    }

    template <typename InputIterator>
    void erase(InputIterator it_start, InputIterator it_end) {
        auto it = it_start;
        while (it_start != it_end) {
            ++it_start;
            erase(it);
            it = it_start;
        }
    }

    Value& operator[](const Key& key) {
        iterator it = find(key);
        if (it != end()) {
            return it->second;
        }
        auto pair = insert({key, Value()});
        return pair.first->second;
    }

    Value& at(const Key& key) {
        auto res = find(key);
        if (res == end()) {
            throw std::range_error("");
        }
        return res->second;
    }

    const Value& at(const Key& key) const {
        return at(key);
    }

    Value& at(Key&& key) {
        auto res = find(std::move(key));
        if (res == end()) {
            throw std::range_error("");
        }
        return res->second;
    }

    const Value& at(Key&& key) const {
        return at(std::move(key));
    }

    size_t size() {
        return inner_list_.size();
    }
};
