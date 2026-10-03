#pragma once

#include "Hominem/Core/Log.h"

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

namespace Hominem {

/// A generic tree stored flat: nodes live in one vector and refer to each other by index.
/// Ids stay valid for the tree's lifetime (no removal yet).
template<typename T>
class Tree
{
public:
    using NodeId = uint32_t;
    static constexpr NodeId Null = ~0u;

    /// Adds a node under `parent`, or as a root when Null.
    NodeId Add(T value, NodeId parent = Null)
    {
        const NodeId id = (NodeId)m_Nodes.size();
        m_Nodes.push_back({ std::move(value) });
        SetParent(id, parent);
        return id;
    }

    /// Moves `node` (with its subtree) under `parent`; Null makes it a root.
    void SetParent(NodeId node, NodeId parent)
    {
        Node& n = m_Nodes[node];
        if (n.parent == parent && (parent != Null || n.isRoot)) return;
        HMN_CORE_ASSERT(parent == Null || (parent != node && !IsAncestor(node, parent)),
                        "Tree: reparenting would create a cycle");

        if (n.parent != Null) std::erase(m_Nodes[n.parent].children, node);
        else if (n.isRoot)    std::erase(m_Roots, node);

        n.parent = parent;
        n.isRoot = parent == Null;
        if (n.isRoot) m_Roots.push_back(node);
        else          m_Nodes[parent].children.push_back(node);
    }

    NodeId                 GetParent(NodeId node) const   { return m_Nodes[node].parent; }
    std::span<const NodeId> GetChildren(NodeId node) const { return m_Nodes[node].children; }
    std::span<const NodeId> GetRoots() const               { return m_Roots; }

    T&       operator[](NodeId node)       { return m_Nodes[node].value; }
    const T& operator[](NodeId node) const { return m_Nodes[node].value; }
    size_t   Size() const                  { return m_Nodes.size(); }

    /// True when `ancestor` is above `node`.
    bool IsAncestor(NodeId ancestor, NodeId node) const
    {
        for (NodeId p = m_Nodes[node].parent; p != Null; p = m_Nodes[p].parent)
            if (p == ancestor) return true;
        return false;
    }

    /// Pre-order walk from every root: a parent is visited before its children.
    /// fn(NodeId node, NodeId parent).
    template<typename Fn>
    void VisitDepthFirst(Fn&& fn) const
    {
        std::vector<NodeId> stack(m_Roots.rbegin(), m_Roots.rend());
        while (!stack.empty())
        {
            const NodeId node = stack.back();
            stack.pop_back();
            fn(node, m_Nodes[node].parent);

            const auto& children = m_Nodes[node].children;
            stack.insert(stack.end(), children.rbegin(), children.rend());
        }
    }

private:
    struct Node
    {
        T                   value;
        NodeId              parent = Null;
        bool                isRoot = false;
        std::vector<NodeId> children;
    };

    std::vector<Node>   m_Nodes;
    std::vector<NodeId> m_Roots;
};

}
