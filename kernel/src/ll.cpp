/*===========================================================================
     _____        _____        _____        _____
 ___|    _|__  __|_    |__  __|__   |__  __| __  |__  ______
|    \  /  | ||    \      ||     |     ||  |/ /     ||___   |
|     \/   | ||     \     ||     \     ||     \     ||___   |
|__/\__/|__|_||__|\__\  __||__|\__\  __||__|\__\  __||______|
    |_____|      |_____|      |_____|      |_____|

--[Mark3 Realtime Platform]--------------------------------------------------

Copyright (c) 2012 - 2017 Funkenstein Software Consulting, all rights reserved.
See license.txt for more information
===========================================================================*/
/*!

    \file   ll.cpp

    \brief  Core Linked-List implementation, from which all kernel objects
            are derived
*/

#include "kerneltypes.h"
#include "kernel.h"
#include "ll.h"

#define _CAN_HAS_DEBUG
//--[Autogenerated - Do Not Modify]------------------------------------------
#include "dbg_file_list.h"
#include "buffalogger.h"
#if defined(DBG_FILE)
#error "Debug logging file token already defined!  Bailing."
#else
#define DBG_FILE _DBG___KERNEL_LL_CPP
#endif
//--[End Autogenerated content]----------------------------------------------

#include "kerneldebug.h"

namespace Mark3
{
//---------------------------------------------------------------------------
void LinkListNode::ClearNode()
{
    next = NULL;
    prev = NULL;
}

//---------------------------------------------------------------------------
void DoubleLinkList::Add(LinkListNode* node_)
{
    KERNEL_ASSERT(node_);

    node_->prev = m_pclTail;
    node_->next = NULL;

    // If the list is empty, initilize the head
    if (m_pclHead == nullptr) {
        m_pclHead = node_;
    }
    // Otherwise, adjust the tail's next pointer
    else {
        m_pclTail->next = node_;
    }

    // Move the tail node, and assign it to the new node just passed in
    m_pclTail = node_;
}

//---------------------------------------------------------------------------
void DoubleLinkList::Remove(LinkListNode* node_)
{
    KERNEL_ASSERT(node_);

    if (node_->prev != nullptr) {
#if SAFE_UNLINK
        if (node_->prev->next != node_) {
            Kernel::Panic(PANIC_LIST_UNLINK_FAILED);
        }
#endif
        node_->prev->next = node_->next;
    }
    if (node_->next != nullptr) {
#if SAFE_UNLINK
        if (node_->next->prev != node_) {
            Kernel::Panic(PANIC_LIST_UNLINK_FAILED);
        }
#endif
        node_->next->prev = node_->prev;
    }
    if (node_ == m_pclHead) {
        m_pclHead = node_->next;
    }
    if (node_ == m_pclTail) {
        m_pclTail = node_->prev;
    }
    node_->ClearNode();
}

//---------------------------------------------------------------------------
void CircularLinkList::Add(LinkListNode* node_)
{
    KERNEL_ASSERT(node_);

    if (m_pclHead == nullptr) {
        // If the list is empty, initilize the nodes
        m_pclHead = node_;
        m_pclTail = node_;
    } else {
        // Move the tail node, and assign it to the new node just passed in
        m_pclTail->next = node_;
    }

    // Add a node to the end of the linked list.
    node_->prev = m_pclTail;
    node_->next = m_pclHead;

    m_pclTail       = node_;
    m_pclHead->prev = node_;
}

//---------------------------------------------------------------------------
void CircularLinkList::Remove(LinkListNode* node_)
{
    KERNEL_ASSERT(node_);

    // Check to see if this is the head of the list...
    if ((node_ == m_pclHead) && (m_pclHead == m_pclTail)) {
        // Clear the head and tail pointers - nothing else left.
        m_pclHead = NULL;
        m_pclTail = NULL;
        return;
    }

#if SAFE_UNLINK
    // Verify that all nodes are properly connected
    if ((node_->prev->next != node_) || (node_->next->prev != node_)) {
        Kernel::Panic(PANIC_LIST_UNLINK_FAILED);
    }
#endif

    // This is a circularly linked list - no need to check for connection,
    // just remove the node.
    node_->next->prev = node_->prev;
    node_->prev->next = node_->next;

    if (node_ == m_pclHead) {
        m_pclHead = m_pclHead->next;
    }
    if (node_ == m_pclTail) {
        m_pclTail = m_pclTail->prev;
    }
    node_->ClearNode();
}

//---------------------------------------------------------------------------
void CircularLinkList::PivotForward()
{
    if (m_pclHead != nullptr) {
        m_pclHead = m_pclHead->next;
        m_pclTail = m_pclTail->next;
    }
}

//---------------------------------------------------------------------------
void CircularLinkList::PivotBackward()
{
    if (m_pclHead != nullptr) {
        m_pclHead = m_pclHead->prev;
        m_pclTail = m_pclTail->prev;
    }
}

//---------------------------------------------------------------------------
void CircularLinkList::InsertNodeBefore(LinkListNode* node_, LinkListNode* insert_)
{
    KERNEL_ASSERT(node_);

    node_->next = insert_;
    node_->prev = insert_->prev;

    if (insert_->prev != nullptr) {
        insert_->prev->next = node_;
    }
    insert_->prev = node_;
}
} //namespace Mark3