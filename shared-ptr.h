#pragma once

#include <cstddef>
#include <memory>

struct ControlBlock {
  size_t weak_ptr_cnt;
  size_t shared_ptr_cnt;

  ControlBlock(size_t weak_ptr_cnt, size_t shared_ptr_cnt)
      : weak_ptr_cnt(weak_ptr_cnt), shared_ptr_cnt(shared_ptr_cnt) {}

  void add_shared() {
    weak_ptr_cnt++;
    shared_ptr_cnt++;
  }

  void remove_shared() {
    weak_ptr_cnt--;
    shared_ptr_cnt--;
  }

  void add_weak() {
    weak_ptr_cnt++;
  }

  void remove_weak() {
    weak_ptr_cnt--;
  }

  virtual ~ControlBlock() {}

  virtual void deleteObjectPtr() = 0;
};

template <class T, class Deleter>
struct ControlBlockWithPointer : ControlBlock {
  T* object_ptr;
  Deleter deleter;
  ControlBlockWithPointer(T* object_ptr, Deleter deleter)
      : ControlBlock(1, 1), object_ptr(object_ptr), deleter(deleter) {}

  void deleteObjectPtr() override {
    this->deleter(object_ptr);
    object_ptr = nullptr;
  }
};

template <class T>
struct ControlBlockWithValue : ControlBlock {
  std::aligned_storage_t<sizeof(T), alignof(T)> object_storage;

  template <class... Args>
  ControlBlockWithValue(Args... args)
      : ControlBlock(1, 1) {
    ::new (&object_storage) T(std::forward<Args>(args)...);
  }

  void deleteObjectPtr() override {
    reinterpret_cast<T*>(&object_storage)->~T();
  }

  ~ControlBlockWithValue() {

  }
};

template <typename T>
class weak_ptr;

template<typename T>
class shared_ptr;

template <typename T>
class shared_ptr {
public:
  friend class weak_ptr<T>;

  template <typename V, typename... Args>
  friend shared_ptr<V> make_shared(Args&&... args);

  template <typename Y>
  friend class shared_ptr;

  shared_ptr() noexcept = default;

  explicit shared_ptr(nullptr_t) noexcept {};

  template<typename V, typename  Deleter = std::default_delete<V>>
  explicit shared_ptr(V* ptr_, Deleter deleter = std::default_delete<V>())
      : object_ptr(ptr_),
        control_block_ptr(new ControlBlockWithPointer{ptr_, deleter}) {}

  template<typename V>
  shared_ptr(const shared_ptr<V>& other, T* object_ptr) : object_ptr(object_ptr), control_block_ptr(other.control_block_ptr) {
    if (control_block_ptr != nullptr) {
      control_block_ptr->add_shared();
    }
  }

  shared_ptr(const shared_ptr& other) noexcept : object_ptr(other.object_ptr), control_block_ptr(other.control_block_ptr) {
    if (control_block_ptr != nullptr) {
      control_block_ptr->add_shared();
    }
  }

  template<class V>
  shared_ptr(const shared_ptr<V>& other) noexcept : object_ptr(other.object_ptr), control_block_ptr(other.control_block_ptr) {
    if (control_block_ptr != nullptr) {
      control_block_ptr->add_shared();
    }
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
    if (control_block_ptr) {
      control_block_ptr->add_shared();
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

  operator T*() const noexcept {
    return object_ptr;
  }

  bool operator==(const shared_ptr& rhs) {
    return rhs.control_block_ptr == control_block_ptr;
  }

  operator bool() const noexcept {
    return object_ptr;
  }

  T& operator*() const noexcept {
    return *object_ptr;
  }

  T* operator->() const noexcept {
    return get();
  }

  std::size_t use_count() const noexcept {
    if (!control_block_ptr) {
      return 0;
    }
    return control_block_ptr->shared_ptr_cnt;
  }

  void reset() noexcept {
    this->~shared_ptr();
  }

  template<class V, class Deleter = std::default_delete<V>>
  void reset(V* new_ptr, Deleter deleter = std::default_delete<V>()) {
    reset();
    object_ptr = new_ptr;
    control_block_ptr = new ControlBlockWithPointer{new_ptr, deleter};
  }

  ~shared_ptr() {
    if (control_block_ptr) {
      control_block_ptr->remove_shared();
      if (control_block_ptr->shared_ptr_cnt == 0) {
        control_block_ptr->deleteObjectPtr();
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
    if (other.control_block_ptr) {
      other.control_block_ptr->add_weak();
    }
  }

  weak_ptr(const weak_ptr<T>& other) noexcept : control_block_ptr(other.control_block_ptr), object_ptr(other.object_ptr) {
    if (other.control_block_ptr) {
      other.control_block_ptr->add_weak();
    }
  }

  weak_ptr& operator=(const weak_ptr<T>& other) noexcept {
    if (control_block_ptr == other.control_block_ptr) {
      return *this;
    }
    this->~weak_ptr();
    control_block_ptr = other.control_block_ptr;
    object_ptr = other.object_ptr;
    if (control_block_ptr) {
      control_block_ptr->add_weak();
    }
    return *this;
  }

  weak_ptr& operator=(weak_ptr<T>&& other) noexcept {
    if (control_block_ptr == other.control_block_ptr) {
      return *this;
    }
    this->~weak_ptr();
    std::swap(this->control_block_ptr, other.control_block_ptr);
    std::swap(this->object_ptr, other.object_ptr);
    return *this;
  }

  weak_ptr& operator=(const shared_ptr<T>& other) noexcept {
    if (control_block_ptr == other.control_block_ptr) {
      return *this;
    }
    this->~weak_ptr();
    control_block_ptr = other.control_block_ptr;
    object_ptr = other.object_ptr;
    if (control_block_ptr) {
      control_block_ptr->add_weak();
    }
    return *this;
  }

  shared_ptr<T> lock() const noexcept {
    if (!control_block_ptr ||
        control_block_ptr->shared_ptr_cnt == 0) {
      return shared_ptr<T>(nullptr);
    }
    auto shared = shared_ptr<T>(nullptr);
    shared.object_ptr = object_ptr;
    shared.control_block_ptr = control_block_ptr;
    control_block_ptr->add_shared();
    return shared;
  }

  ~weak_ptr() {
    if (control_block_ptr) {
      control_block_ptr->remove_weak();
      if (control_block_ptr->weak_ptr_cnt == 0) {
        delete control_block_ptr;
      }
      control_block_ptr = nullptr;
      object_ptr = nullptr;
    }
  }

private:
  ControlBlock * control_block_ptr = nullptr;
  T* object_ptr = nullptr;
};

template <typename T, typename... Args>
shared_ptr<T> make_shared(Args&&... args) {
  auto controlBlock = new ControlBlockWithValue<T>(args...);
  auto shared = shared_ptr<T>(nullptr);
  shared.control_block_ptr = controlBlock;
  shared.object_ptr = reinterpret_cast<T*>(&controlBlock->object_storage);
  return shared;
}
