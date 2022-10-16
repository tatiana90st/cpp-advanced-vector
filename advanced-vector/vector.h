#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <type_traits>
#include <algorithm>


template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;
    RawMemory(RawMemory&& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }
    RawMemory& operator=(RawMemory&& rhs) noexcept {
        buffer_ = std::exchange(rhs.buffer_, nullptr);
        capacity_ = std::exchange(rhs.capacity_, 0);
        return *this;
    }

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    Vector() = default;
    
    explicit Vector(size_t size)
        : data_(size)
        , size_(size) //
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_) //
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept
        :data_(std::move(other.data_))
        , size_(std::exchange(other.size_, 0)) {

    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            }
            else {

                if (rhs.size_ < size_) {
                    /*Размер вектора-источника меньше размера вектора-приёмника. Тогда нужно скопировать имеющиеся элементы из источника в приёмник, 
                    а избыточные элементы вектора-приёмника разрушить:*/
                    //для copy нужен итератор
                    size_t i = 0;
                    for (; i != rhs.size_; ++i) {
                        data_[i] = rhs.data_[i];
                    }
                    std::destroy_n(data_ + i, size_ - i);
                }
                else {
                    /*Размер вектора-источника больше или равен размеру вектора-приёмника. 
                    //Тогда нужно присвоить существующим элементам приёмника значения соответствующих элементов источника, 
                    //а оставшиеся скопировать в свободную область, используя функцию uninitialized_copy или uninitialized_copy_n.*/
                    size_t i = 0;
                    for (; i != size_; ++i) {
                        data_[i] = rhs.data_[i];
                    }
                    std::uninitialized_copy_n(rhs.data_ + i, rhs.size_ - i, data_ + i);
                }
            }
        }
        size_ = rhs.size_;
        return *this;
    }
    Vector& operator=(Vector&& rhs) noexcept {
        data_ = std::move(rhs.data_);
        size_ = std::exchange(rhs.size_, 0);
        return *this;
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return data_ + size_;
    }
    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator end() const noexcept {
        return data_ + size_;
    }
    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator cend() const noexcept {
        return data_ + size_;
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        MoveOrCopy(data_.GetAddress(), new_data.GetAddress(), size_);
        // Избавляемся от старой сырой памяти, обменивая её на новую
        data_.Swap(new_data);
        // При выходе из метода старая память будет возвращена в кучу
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    void Resize(size_t new_size) {
        if (new_size == size_) {
            return;
        }
        if (new_size < size_) {
            //При уменьшении размера вектора нужно удалить лишние элементы вектора, 
            //вызвав их деструкторы
            std::destroy_n(data_ + new_size, size_ - new_size);
        }
        else {
            //сначала нужно убедиться, что вектору достаточно памяти для новых элементов
            //затем новые элементы нужно проинициализировать
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_ + size_, new_size - size_);
        }
        size_ = new_size;
    }

    
    void PushBack(const T& value) {
        EmplaceBack(value);
    }

    void PushBack(T&& value) {
        EmplaceBack(std::move(value));
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        T* t = nullptr;
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            t = new (new_data + size_) T(std::forward<Args>(args)...);
            MoveOrCopy(data_.GetAddress(), new_data.GetAddress(), size_);
            data_.Swap(new_data);
        }
        else {
            t = new (data_ + size_) T(std::forward<Args>(args)...);
        }
        ++size_;
        return *t;
    }
    //Методы Emplace и Erase, выполняя реаллокацию, должны перемещать элементы, а не копировать, если действительно любое из условий:
    //тип T имеет noexcept - конструктор перемещения;
    //тип T не имеет конструктора копирования.
    //Если при вставке происходит реаллокация, ожидается ровно Size() перемещений или копирований существующих элементов. 
    //Сам вставляемый элемент должен копироваться либо перемещаться в зависимости от версии метода Insert.

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {

        if (pos == end()) {
            return &EmplaceBack(std::forward<Args>(args)...);
        }

        size_t index = pos - begin();
        if (size_ != Capacity()) {
            //Вызов Insert и Emplace, не требующий реаллокации, 
            //должен перемещать ровно end()-pos существующих элементов плюс одно перемещение элемента из временного объекта в вектор.

            //скопировать значения во временный объект
            T temp_obj (std::forward<Args>(args)...);
            //переместить последний элемент в неиниц память
            size_t last_el = size_ - 1;
            new (end()) T(std::move(data_[last_el]));
            //Затем переместить элементы диапазона [pos, end()-1) вправо на один элемент
            std::move_backward(data_ + index, data_ + last_el, end());

            //После перемещения элементов нужно переместить временное значение во вставляемую позицию
            data_[index] = std::move(temp_obj);
        }
        else {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data + index) T(std::forward<Args>(args)...);
            try {
                //Затем копируются либо перемещаются элементы, которые предшествуют вставленному элементу
                MoveOrCopy(data_.GetAddress(), new_data.GetAddress(), index);
            }
            catch (...) {
                //Если исключение выбросится при их копировании, нужно разрушить ранее вставленный элемент в обработчике
                new_data[index].~T();
                throw;
            }
            try {
                //Затем копируются либо перемещаются элементы, которые следуют за вставляемым
                MoveOrCopy(data_ + index, new_data + (index + 1), size_ - index);
            }
            catch (...) {
                //Если выбросится исключение при конструировании элементов, следующих за pos, ренее скопированные элементы нужно удалить
                std::destroy_n(new_data.GetAddress(), index + 1);
            }
            data_.Swap(new_data);
        }
        ++size_;
        return data_ + index;
    }
  
    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }
    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    iterator Erase(const_iterator pos) {
        if (pos == end()) {
            PopBack();
            return end();
        }
        //Сначала на место удаляемого элемента нужно переместить следующие за ним элементы
        size_t index = pos - begin();
        std::move(data_ + index + 1, end(), data_ + index);
        //После разрушения объекта в конце вектора и обновления поля size_ удаление элемента можно считать завершённым
        PopBack();
        return data_ + index;
    }

    void PopBack() {
        std::destroy_n(data_ + size_- 1, 1);
        --size_;
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;

    void MoveOrCopy(T* from, T* to, size_t this_many) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(from, this_many, to);
        }
        else {
            std::uninitialized_copy_n(from, this_many, to);
        }
        std::destroy_n(from, this_many);
    }
};