#pragma once

#include <cstddef>

struct ControlBlock {
  size_t weak_ptr_cnt;
  size_t shared_ptr_cnt;
};

template <typename T>
class weak_ptr;

template <typename T>
class shared_ptr {
public:
  friend class weak_ptr<T>;

  shared_ptr() noexcept = default;

  shared_ptr(nullptr_t) noexcept {};

  shared_ptr(T* ptr_)
      : object_ptr(ptr_), control_block_ptr(new ControlBlock{1, 1}) {}

  shared_ptr(const shared_ptr& other) noexcept {
    *this = other;
  }

  shared_ptr(shared_ptr&& other) noexcept
      : object_ptr(other.object_ptr),
        control_block_ptr(other.control_block_ptr) {
    other.object_ptr = nullptr;
    other.control_block_ptr = nullptr;
  }

  shared_ptr& operator=(const shared_ptr& other) noexcept {
    if (this == &other) {
      return *this;
    }
    this->~shared_ptr();
    this->object_ptr = other.object_ptr;
    this->control_block_ptr = other.control_block_ptr;
    if (control_block_ptr != nullptr) {
      control_block_ptr->weak_ptr_cnt++;
      control_block_ptr->shared_ptr_cnt++;
    }
    return *this;
  }

  shared_ptr& operator=(shared_ptr&& other) noexcept {
    if (this == &other) {
      return *this;
    }
    this->~shared_ptr();
    std::swap(control_block_ptr, other.control_block_ptr);
    std::swap(object_ptr, other.object_ptr);
    return *this;
  }

  T* get() const noexcept {
    return object_ptr;
  }

  operator bool() const noexcept {
    return object_ptr != nullptr;
  }

  T& operator*() const noexcept {
    return *object_ptr;
  }

  T* operator->() const noexcept {
    return get();
  }

  std::size_t use_count() const noexcept {
    if (control_block_ptr == nullptr) {
      return 0;
    }
    return control_block_ptr->shared_ptr_cnt;
  }

  void reset() noexcept {
    this->~shared_ptr();
  }

  void reset(T* new_ptr) {
    reset();
    object_ptr = new_ptr;
    control_block_ptr = new ControlBlock{1, 1};
  }

  ~shared_ptr() {
    if (control_block_ptr != nullptr) {
      control_block_ptr->shared_ptr_cnt--;
      control_block_ptr->weak_ptr_cnt--;
      if (control_block_ptr->shared_ptr_cnt == 0) {
        delete object_ptr;
      }
      if (control_block_ptr->weak_ptr_cnt == 0) {
        delete control_block_ptr;
      }
      control_block_ptr = nullptr;
      object_ptr = nullptr;
    }
  }

private:
  T* object_ptr = nullptr;
  ControlBlock* control_block_ptr = nullptr;
};

template <typename T>
class weak_ptr {
public:
  weak_ptr() noexcept = default;

  weak_ptr(const shared_ptr<T>& other) noexcept
      : control_block_ptr(other.control_block_ptr), object_ptr(other.get()) {
    if (other.control_block_ptr != nullptr) {
      other.control_block_ptr->weak_ptr_cnt++;
    }
  }
  weak_ptr& operator=(const shared_ptr<T>& other) noexcept {
    if (control_block_ptr == other.control_block_ptr) {
      return *this;
    }
    this->~weak_ptr();
    control_block_ptr = other.control_block_ptr;
    object_ptr = other.object_ptr;
    if (control_block_ptr != nullptr) {
      control_block_ptr->weak_ptr_cnt++;
    }
    return *this;
  }

  shared_ptr<T> lock() const noexcept {
    if (control_block_ptr == nullptr ||
        control_block_ptr->shared_ptr_cnt == 0) {
      return shared_ptr<T>(nullptr);
    }
    auto shared = shared_ptr<T>(nullptr);
    shared.object_ptr = object_ptr;
    shared.control_block_ptr = control_block_ptr;
    control_block_ptr->shared_ptr_cnt++;
    control_block_ptr->weak_ptr_cnt++;
    return shared;
  }

  ~weak_ptr() {
    if (control_block_ptr != nullptr) {
      control_block_ptr->weak_ptr_cnt--;
      if (control_block_ptr->weak_ptr_cnt == 0) {
        delete control_block_ptr;
      }
      control_block_ptr = nullptr;
      object_ptr = nullptr;
    }
  }

private:
  ControlBlock* control_block_ptr = nullptr;
  T* object_ptr = nullptr;
};

template <typename T, typename... Args>
shared_ptr<T> make_shared(Args&&... args) {
  return shared_ptr<T>(new T(std::forward<Args>(args)...));
}
