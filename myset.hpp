
#pragma once

#include <set>
#include <functional>

namespace ganttry
{

template<typename T>
struct DefaultComp
{
    bool operator()(const T& left, const T& right)
    {
        return left.get_id() < right.get_id();
    }
    template<typename W>
    bool operator()(const T& left, const W& right)
    {
        return left.get_id() < right;
    }
    template<typename W>
    bool operator()(const W& right, const T& left)
    {
        return left < right.get_id();
    }
};
template<typename T>
struct PtrComp
{
    bool operator()(const T& left, const T& right)
    {
        return left->get_id() < right->get_id();
    }
    template<typename W>
    bool operator()(const T& left, const W& right)
    {
        return left->get_id() < right;
    }
    template<typename W>
    bool operator()(const W& right, const T& left)
    {
        return left < right->get_id();
    }
};

template<typename T>
struct DefaultIDSetter
{
    void operator()(T & t, size_t id) { t.set_id(id); }
};
template<typename T>
struct PtrIDSetter
{
    void operator()(T & t, size_t id) { t->set_id(id); }
};

template<typename T, typename Setter=DefaultIDSetter<T>, typename Comp=DefaultComp<T>>
class setid : public std::set<T,Comp>
{
    using Super = std::set<T,Comp>;
    using value_type  = typename Super::value_type;
    using iterator    = typename Super::iterator;

    size_t next_id_ = 0;
    Setter setter;

public:
    size_t get_next_id() { return next_id_; }

    std::pair<iterator, bool> insert(T && value)
    {
        setter(value, next_id_);

        iterator it;
        bool b;
        std::tie(it,b) = Super::insert(std::move(value));

        if (b)
            next_id_++;

        return {it,b};
    }

    template<typename K>
    T * find(K t)
    {
        auto it = Super::find(T::make_dummy(t));
        if (it == this->end())
            return nullptr;
        else
            return const_cast<T*>(&*it);
    }
};

}
