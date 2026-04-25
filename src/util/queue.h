// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once
/// @author Alex Conesa, 2024-2025
/// @brief A simple fixed-size queue implementation.
/// @details This queue is designed to be used in scenarios where the maximum number of items is
/// known at compile time.
///          It uses a circular buffer to efficiently manage the items.

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace ungula {

    template <typename T, size_t Capacity>
    class Queue {
        private:
            size_t front_ = 0;
            size_t back_ = 0;
            size_t count_ = 0;
            T data_[Capacity];

        public:
            constexpr size_t capacity() const noexcept {
                return Capacity;
            }
            constexpr bool isFull() const noexcept {
                return count_ == Capacity;
            }
            constexpr bool isEmpty() const noexcept {
                return count_ == 0;
            }

            bool push(const T& item) noexcept {
                if (isFull())
                    return false;
                data_[back_] = item;
                back_ = (back_ + 1) % Capacity;
                ++count_;
                return true;
            }
            bool push(T&& item) noexcept {
                if (isFull())
                    return false;
                data_[back_] = std::move(item);
                back_ = (back_ + 1) % Capacity;
                ++count_;
                return true;
            }

            // returns false if empty
            bool pop(T& out) noexcept {
                if (isEmpty())
                    return false;
                out = std::move(data_[front_]);
                front_ = (front_ + 1) % Capacity;
                --count_;
                return true;
            }

            // peek without removing
            bool peek(T& out) const noexcept {
                if (isEmpty())
                    return false;
                out = data_[front_];
                return true;
            }

            void clear() noexcept {
                front_ = back_;
                count_ = 0;
            }

            size_t count() const noexcept {
                return count_;
            }
    };

}  // namespace ungula
