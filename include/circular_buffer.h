#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <stddef.h>

template <class T, size_t S>
class CircularBuffer final
{
  public:
  explicit CircularBuffer() : buf_{}, max_size_(S)
  {
  }

  void put(T& item);
  T get();
  void reset();
  bool empty() const;
  bool full() const;
  size_t capacity() const;
  size_t size() const;

  private:
  T buf_[S];
  size_t head_ = 0;
  size_t tail_ = 0;
  size_t const max_size_;
  bool full_ = 0;
};

template <class T, size_t S>
void CircularBuffer<T, S>::put(T& item)
{
  buf_[head_] = item;
  if (full_)
  {
    tail_ = (tail_ + 1) % max_size_;
  }
  head_ = (head_ + 1) % max_size_;
  full_ = head_ == tail_;
}

template <class T, size_t S>
T CircularBuffer<T, S>::get()
{
  if (empty())
  {
    return T();
  }

  // Read data and advance the tail (we now have a free space)
  auto val = buf_[tail_];
  full_ = false;
  tail_ = (tail_ + 1) % max_size_;

  return val;
}

template <class T, size_t S>
void CircularBuffer<T, S>::reset()
{
  head_ = tail_;
  full_ = false;
}

template <class T, size_t S>
bool CircularBuffer<T, S>::empty() const
{
  return (!full_ && (head_ == tail_));
}

template <class T, size_t S>
bool CircularBuffer<T, S>::full() const
{
  // If tail is ahead the head by 1, we are full
  return full_;
}

template <class T, size_t S>
size_t CircularBuffer<T, S>::capacity() const
{
  return max_size_;
}

template <class T, size_t S>
size_t CircularBuffer<T, S>::size() const
{
  size_t size = max_size_;

  if (!full_)
  {
    if (head_ >= tail_)
    {
      size = head_ - tail_;
    }
    else
    {
      size = max_size_ + head_ - tail_;
    }
  }

  return size;
}

#endif /* CIRCULAR_BUFFER_H */