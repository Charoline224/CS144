#pragma once

#include <cstdint>
#include <string>
#include <string_view>


class CircleQueue{
public:
    explicit CircleQueue(std::size_t capacity);
    void push(std::string data);
    void pop(int num);
    int leftspace() const;
    int occupiedspace() const;
    std::string_view getview() const;
    bool is_empty() const;


protected:
    std::string str;
    bool full_;
    int size;
    int head;
    int tail;
};