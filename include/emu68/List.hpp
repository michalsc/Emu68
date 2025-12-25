/*
    Copyright © 2019-2025 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <emu68/Node>

namespace Emu68 {

template <class T = Node>
class NodeIterator
{
    T* ni_node;
public:
    NodeIterator(Node* node) : ni_node(static_cast<T*>(node)) {}
    NodeIterator(const NodeIterator& it) : ni_node(it.ni_node) {}
    NodeIterator& operator++() { ni_node = static_cast<T*>(ni_node->next()); return *this; }
    NodeIterator& operator--() { ni_node = static_cast<T*>(ni_node->prev()); return *this; }
    NodeIterator operator++(int) { NodeIterator tmp(*this); operator++(); return tmp; }
    NodeIterator operator--(int) { NodeIterator tmp(*this); operator--(); return tmp; }
    bool operator==(const NodeIterator& rhs) const { return ni_node == rhs.ni_node; }
    bool operator!=(const NodeIterator& rhs) const { return ni_node != rhs.ni_node; }
    T* operator*() { return ni_node; }
};

template <class T = Node>
class List
{
    Node l_head;
    Node l_tail;

public:
    List() {
        l_head.setNext(&l_tail);
        l_head.setPrev(nullptr);
        
        l_tail.setPrev(&l_head);
        l_tail.setNext(nullptr);
    }

    T* addHead(T* n) {
        if (n == nullptr) { return nullptr; }

        n->setPrev(&l_head);
        l_head.next()->setPrev(n);

        n->setNext(l_head.next());
        l_head.setNext(n);
        
        return n;
    }

    T* addTail(T* n) {
        if (n == nullptr) { return nullptr; }

        n->setNext(&l_tail);
        n->setPrev(l_tail.prev());

        l_tail.prev()->setNext(n);
        l_tail.setPrev(n);

        return n;
    }

    bool isHead(T* n) { return n->prev() == &l_head; }
    bool isTail(T* n) { return n->next() == &l_tail; }
    bool isEmpty() { return l_head.next() == &l_tail; }

    T* getHead() {
        if (l_head.next() == &l_tail) {
            return nullptr;
        } else {
            return static_cast<T*>(l_head.next());
        }
    }

    NodeIterator<T> begin() { return NodeIterator<T>(static_cast<T*>(l_head.next())); }
    NodeIterator<T> end() { return NodeIterator<T>(&l_tail); }

    T* getTail() {
        if (l_tail.prev() == &l_head) {
            return nullptr;
        } else {
            return static_cast<T*>(l_tail.prev());
        }
    }

    T* remHead() {
        T* n = getHead();
        if (n != nullptr) { n->remove(); }

        return n;
    }

    T* remTail() {
        T* n = getTail();
        if (n != nullptr) { n->remove(); }

        return n;
    }

};

} // namespace Emu68
