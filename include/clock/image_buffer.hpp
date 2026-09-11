#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "../constants.hpp"

// Implements a ring buffer (FIFO) that stores up to `MAX_IMAGES_TO_BUFFER` images at once
// in a contiguous byte buffer. Images are pushed (write) at `tail` and popped (read) from `head`.
class ImageBuffer {
    public:    
        size_t head = 0; // Index of the image to read/pop.
        size_t tail = 0; // Index of the image to write/push.
        size_t size = 0; // Number of images in the buffer.
        std::vector<unsigned char> data = std::vector<unsigned char>(BUFFER_SIZE, 0); // The buffer where images are stored.

        // Returns the byte offset for the head.
        size_t getReadOffset () const {
            return head * IMAGE_SIZE;
        }

        // Returns the byte offset for the tail.
        size_t getWriteOffset () const {
            return tail * IMAGE_SIZE;
        }

        // Returns true if the buffer currently stores no images.
        bool isEmpty() const {
            return size == 0;
        }

        // Returns true if the buffer is currently at its maximum capacity.
        bool isFull() const {
            return size == MAX_IMAGES_TO_BUFFER;
        }

        // Copies an image into the buffer at the current `tail` slot and advances `tail`.
        //
        // image: The image's pixel data. Must be exactly IMAGE_SIZE bytes long.
        //
        // Throws `std::runtime_error` if the buffer is already full, or if `image` is not exactly IMAGE_SIZE bytes long.
        void pushImage(const std::vector<unsigned char>& image);

        // Pops the oldest buffered image and returns a pointer to it, advancing `head` to the next slot.
        //
        // Throws `std::runtime_error` if the buffer is empty.
        //
        // Returns a pointer to the popped image's data, `IMAGE_SIZE` bytes long.
        std::uint8_t* popImage();
};
