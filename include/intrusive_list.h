/*
   Copyright (c) 2019, 2020, MariaDB

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; version 2 of
   the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1335  USA
*/

#pragma once

#include <cstddef>
#include <iterator>

namespace intrusive
{

// Derive your class from this struct to insert to a linked list.
template <class Tag= void> struct list_node
{
  list_node()
      :
#ifndef NDEBUG
        next(NULL), prev(NULL)
#endif
  {
  }

  list_node(list_node *next, list_node *prev) : next(next), prev(prev) {}

  list_node *next;
  list_node *prev;
};

// Modelled after std::list<T>
template <class T, class Tag= void> class list
{
public:
  typedef list_node<Tag> ListNode;
  class Iterator;

  // All containers in C++ should define these types to implement generic
  // container interface.
  typedef T value_type;
  typedef std::size_t size_type;
  typedef std::ptrdiff_t difference_type;
  typedef value_type &reference;
  typedef const value_type &const_reference;
  typedef T *pointer;
  typedef const T *const_pointer;
  typedef Iterator iterator;
  typedef const Iterator const_iterator;
  typedef std::reverse_iterator<iterator> reverse_iterator;
  typedef std::reverse_iterator<const iterator> const_reverse_iterator;

  class Iterator
  {
  public:
    // All iterators in C++ should define these types to implement generic
    // iterator interface.
    typedef std::bidirectional_iterator_tag iterator_category;
    typedef T value_type;
    typedef std::ptrdiff_t difference_type;
    typedef T *pointer;
    typedef T &reference;

    Iterator(ListNode *node) : node_(node) {}

    Iterator &operator++()
    {
      node_= node_->next;
      return *this;
    }
    Iterator operator++(int)
    {
      Iterator tmp(*this);
      operator++();
      return tmp;
    }

    Iterator &operator--()
    {
      node_= node_->prev;
      return *this;
    }
    Iterator operator--(int)
    {
      Iterator tmp(*this);
      operator--();
      return tmp;
    }

    reference operator*() { return *static_cast<pointer>(node_); }
    pointer operator->() { return static_cast<pointer>(node_); }

    bool operator==(const Iterator &rhs) { return node_ == rhs.node_; }
    bool operator!=(const Iterator &rhs) { return !(*this == rhs); }

  private:
    ListNode *node_;

    friend class list;
  };

  list() : sentinel_(&sentinel_, &sentinel_) {}

  reference front() { return *begin(); }
  reference back() { return *--end(); }

  iterator begin() { return iterator(sentinel_.next); }
  const_iterator begin() const
  {
    return iterator(const_cast<ListNode *>(sentinel_.next));
  }
  iterator end() { return iterator(&sentinel_); }
  const_iterator end() const
  {
    return iterator(const_cast<ListNode *>(&sentinel_));
  }

  reverse_iterator rbegin() { return reverse_iterator(end()); }
  const_reverse_iterator rbegin() const { return reverse_iterator(end()); }
  reverse_iterator rend() { return reverse_iterator(begin()); }
  const_reverse_iterator rend() const { return reverse_iterator(begin()); }

  bool empty() const { return sentinel_.next == &sentinel_; }

  // Not implemented because it's O(N)
  // size_type size() const
  // {
  //   return static_cast<size_type>(std::distance(begin(), end()));
  // }

  void clear()
  {
    sentinel_.next= &sentinel_;
    sentinel_.prev= &sentinel_;
  }

  iterator insert(iterator pos, reference value)
  {
    ListNode *curr= pos.node_;
    ListNode *prev= pos.node_->prev;

    prev->next= &value;
    curr->prev= &value;

    static_cast<ListNode &>(value).prev= prev;
    static_cast<ListNode &>(value).next= curr;

    return iterator(&value);
  }

  iterator erase(iterator pos)
  {
    ListNode *prev= pos.node_->prev;
    ListNode *next= pos.node_->next;

    prev->next= next;
    next->prev= prev;

#ifndef NDEBUG
    ListNode *curr= pos.node_;
    curr->prev= NULL;
    curr->next= NULL;
#endif

    return next;
  }

  void push_back(reference value) { insert(end(), value); }
  void pop_back() { erase(end()); }

  void push_front(reference value) { insert(begin(), value); }
  void pop_front() { erase(begin()); }

  // STL version is O(n) but this is O(1) because an element can't be inserted
  // several times in the same intrusive list.
  void remove(reference value) { erase(iterator(&value)); }

private:
  ListNode sentinel_;
};

// Similar to intrusive::list but also has O(1) size() method.
template <class T, class Tag= void> class sized_list : public list<T, Tag>
{
  typedef list<T, Tag> BASE;

public:
  // All containers in C++ should define these types to implement generic
  // container interface.
  using typename BASE::value_type;
  using typename BASE::size_type;
  using typename BASE::difference_type;
  using typename BASE::reference;
  using typename BASE::const_reference;
  using typename BASE::pointer;
  using typename BASE::const_pointer;
  using typename BASE::iterator;
  using typename BASE::const_iterator;
  using typename BASE::reverse_iterator;
  using typename BASE::const_reverse_iterator;

  sized_list() : size_(0) {}

  size_type size() const { return size_; }

  void clear()
  {
    BASE::clear();
    size_= 0;
  }

  iterator insert(iterator pos, reference value)
  {
    ++size_;
    return BASE::insert(pos, value);
  }

  iterator erase(iterator pos)
  {
    --size_;
    return BASE::erase(pos);
  }

  void push_back(reference value) { insert(BASE::end(), value); }
  void pop_back() { erase(BASE::end()); }

  void push_front(reference value) { insert(BASE::begin(), value); }
  void pop_front() { erase(BASE::begin()); }

  void remove(reference value) { erase(iterator(&value)); }

private:
  size_t size_;
};

} // namespace intrusive
