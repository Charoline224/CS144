#include "circle_queue.hh"
#include <string>
#include <algorithm>

using namespace std;


CircleQueue::CircleQueue(std::size_t capacity)
    : str(capacity, '\0'),
    full_(false),
    size(capacity),
    head(0),
    tail(0)
    {}

void CircleQueue::push(string data){
    int cnt = min(data.size(), static_cast<std::size_t>(leftspace()));
    int i = 0;
    int avai = leftspace();
    if(head < tail){
        int num = str.size() - tail;
        if (num > cnt){
            i += cnt;
            copy_n(data.begin(), cnt, str.begin() + tail);
            tail += cnt;
        }else{
            i += num;
            copy_n(data.begin(), num, str.begin() + tail);
            tail = 0;
        }
    }
    while(leftspace() && i < cnt ){
        str[tail++] = data[i++];
        if(tail >= size) tail -= size;
    }
    if(cnt > 0 && avai == cnt)full_ = true;

}

void CircleQueue::pop(int num){
    if (is_empty()){
        return;
    }
    int len = min(num, occupiedspace());
    if (head < tail){
       head += len;
    }else{
        if((size - head) >= len){
            head += len;
        }else{
            head = len - (size - head);
        }
    }
    if(head == size)head = 0;
    if(num)full_ = false;
    return ;
}

int CircleQueue::leftspace() const{
    if (head < tail || is_empty()){
        return size - (tail-head);
    }else{
        return head - tail ;
    }
}

int CircleQueue::occupiedspace() const{
    if (head < tail || is_empty()){
        return tail-head;
    }else{
        return size - (head - tail);
    }
}


std::string_view CircleQueue::getview() const{
    if (is_empty()) {
        return {};
    }

    const std::size_t buffered = static_cast<std::size_t>(occupiedspace());
    const std::size_t contiguous = str.size() - static_cast<std::size_t>(head);
    const std::size_t len = std::min(buffered, contiguous);

    return {str.data() + head, len};
}

bool CircleQueue::is_empty()const{
    return ((head == tail) && !full_);
}