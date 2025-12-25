#pragma once

namespace Emu68 {

class Node {
    Node* n_prev;
    Node* n_next;
    
public:
    Node* prev() { return n_prev; }
    Node* next() { return n_next; }
    void setPrev(Node* p) { n_prev = p; }
    void setNext(Node* n) { n_next = n; }
    void remove();
};

inline void Node::remove() {
    n_prev->n_next = n_next;
    n_next->n_prev = n_prev;
}

} // namespace Emu68
