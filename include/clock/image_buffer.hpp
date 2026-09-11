#pragma once
// A circular buffer for images

#include <cstddef>
#include <cstdint>
#include <vector>

#include "../constants.hpp"

class ImageBuffer {
    public:    
        size_t head = 0;
        size_t tail = 0;
        size_t size = 0;
        std::vector<unsigned char> data = std::vector<unsigned char>(BUFFER_SIZE, 0);

        // byte offset for the head
        size_t getReadOffset () const {
            return head * IMAGE_SIZE;
        }

        // byte offset for the tail
        size_t getWriteOffset () const {
            return tail * IMAGE_SIZE;
        }

        size_t nextSlot() const {
            return (head + 1) % MAX_IMAGES_TO_BUFFER;
        }

        bool isEmpty() {
            return size == 0;
        }

        bool isFull() {
            return size == MAX_IMAGES_TO_BUFFER;
        }

        // push an image onto the buffer
        void pushImage(const std::vector<unsigned char>& image);

        // pop an image from the buffer
        std::uint8_t* popImage();
};
