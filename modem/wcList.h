/*
  wcList.h - double linked list
  Stefan Wessels, 2022
*/
#ifndef wclist_h
#define wclist_h

//----------------------------------------------------------------------------
// Forward declerations
template <class T>
class wcList;
template <class T>
class wcNameList;

//----------------------------------------------------------------------------
// Basic node to keep in wcList's
template <class T>
class wcNode
{
public:
    T *m_next;
    T *m_prev;

public:
    wcNode() : m_next(0), m_prev(0) { ; }
    virtual ~wcNode() { ; }

    T *GetNext(void) const { return m_next; }
    T *GetPrev(void) const { return m_prev; }
};

//----------------------------------------------------------------------------
// Basic list class
template <class T>
class wcList
{
private:
    T *m_head;
    T *m_tail;

public:
    wcList() : m_head(0), m_tail(0) { ; }
    virtual ~wcList() { FlushList(); }

    T *AddNode(T *insertAfterNode, T *aNode);
    T *AddHead(T *aNode) { return AddNode(NULL, aNode); }
    T *AddTail(T *aNode) { return AddNode(m_tail, aNode); }
    void AddSorted(T *aNode, int (*SortFunc)(T *, T *));

    T *GetHead(void) const { return m_head; }
    T *GetTail(void) const { return m_tail; }

    T *RemNode(T *aNode);
    T *FindNode(const uint aNodeNum);
    uint NumElements(void) const
    {
        uint c = 0;
        T *n = GetHead();
        while ((n = n->GetNext()))
        {
            c++;
        }
        return c;
    }
    void FlushList(void)
    {
        while (m_head)
        {
            m_tail = dynamic_cast<T *>(m_head->m_next);
            delete m_head;
            m_head = m_tail;
        }
    }
    bool IsEmpty() { return NULL == m_head; }

    friend class wcNameList<T>;
};

//----------------------------------------------------------------------------
// AddNode inserts the node "aNode" after the node "insertAfterNode"
// IN:  insertAfterNode <null or a wcNode> and aNode
// OUT: aNode is in the list after "insertAfterNode" (or a the head if insertAfterNode==NULL)
template <class T>
T *wcList<T>::AddNode(T *insertAfterNode, T *aNode)
{
    if (insertAfterNode)
    {
        aNode->m_prev = insertAfterNode;
        if ((aNode->m_next = insertAfterNode->m_next))
            aNode->m_next->m_prev = aNode;
        else
            m_tail = aNode;
        insertAfterNode->m_next = aNode;
    }
    else
    {
        if ((aNode->m_next = m_head))
            m_head->m_prev = aNode;
        else
            m_tail = aNode;
        m_head = aNode;
    }

    return aNode;
}

//----------------------------------------------------------------------------
// Add a node in sorted order, so that: head > aNode >= tail
// IN:  The node to insert and a function to call on nodes in the list and
//      the aNode to insert
// OUT:
template <class T>
void wcList<T>::AddSorted(T *aNode, int (*SortFunc)(T *, T *))
{
    T *current = m_head;

    assert(aNode->m_next == NULL && aNode->m_prev == NULL);

    // As long as the working node is valid, and the "value"
    // of the working node is greater than that of the new node
    // move along the list
    while (current && SortFunc(aNode, current) > 0)
        current = dynamic_cast<T *>(current->m_next);

    // If the working node is valid, the new node goes into the
    // list before the tail.  Otherwise, it has to become the tail
    if (current)
    {
        // Do an insert before current here

        // Point the new node a the node before the working node
        // If that is NULL, the working node was the head so make the
        // head the new node, otherwise set the m_next of the node
        // before the working node to be the new node
        if ((aNode->m_prev = dynamic_cast<T *>(current->m_prev)))
            current->m_prev->m_next = aNode;
        else
            m_head = aNode;

        // Point the m_next of the new node a the working node
        aNode->m_next = dynamic_cast<T *>(current);
        // ling the working node m_previous back to the new node
        current->m_prev = aNode;
    }
    else
    {
        // Point the m_previous of the new node a the tail.  If
        // the tail was null, assign the head to the new node as well
        // otherwise point the m_next of the tail a the new node
        if ((aNode->m_prev = dynamic_cast<T *>(m_tail)))
            m_tail->m_next = aNode;
        else
            m_head = aNode;

        // make the new node the tail
        m_tail = aNode;
    }
}

//----------------------------------------------------------------------------
// Remove a node from a list and set the aNode m_next and m_prev to NULL
// IN:  a pointer to a node that is in the list
// OUT:
template <class T>
T *wcList<T>::RemNode(T *aNode)
{
    if (aNode == m_head)
    {
        if ((m_head = dynamic_cast<T *>(aNode->m_next)))
            m_head->m_prev = NULL;
        else
            m_tail = NULL;
    }
    else
    {
        if ((aNode->m_prev->m_next = dynamic_cast<T *>(aNode->m_next)))
            aNode->m_next->m_prev = dynamic_cast<T *>(aNode->m_prev);
        else
            m_tail = dynamic_cast<T *>(aNode->m_prev);
    }
    aNode->m_next = aNode->m_prev = NULL;
    return aNode;
}

//----------------------------------------------------------------------------
// IN:  a number for the node to find (0 == head)
// OUT: the nodeNum'th node or NULL if not found
template <class T>
T *wcList<T>::FindNode(uint aNodeNum)
{
    T *current = m_head;
    uint seekNum = aNodeNum;

    while (seekNum-- && current)
        current = dynamic_cast<T *>(current->m_next);

    return current;
}

#endif //wclist_h
