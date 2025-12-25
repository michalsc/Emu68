/*
    Copyright © 2019-2025 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

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
