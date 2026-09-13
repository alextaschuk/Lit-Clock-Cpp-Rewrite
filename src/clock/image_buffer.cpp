#include "clock/image_buffer.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

#include "constants.hpp"

void ImageBuffer::pushImage(const std::vector<unsigned char>& image)
{
    if (isFull()) {
        throw std::runtime_error("Image buffer is full.");
    }
    
    if (image.size() != IMAGE_SIZE_4BPP) {
        throw std::runtime_error("Image size does not match expected IMAGE_SIZE_4BPP.");
    }

    std::copy(image.begin(), image.end(), data.begin() + getWriteOffset());
    tail = (tail + 1) % MAX_IMAGES_TO_BUFFER;
    ++size;
}

uint8_t* ImageBuffer::popImage()
{
    if (isEmpty()) {
        throw std::runtime_error("Image buffer is empty.");
    }

    uint8_t* imagePointer = data.data() + getReadOffset();
    head = (head + 1) % MAX_IMAGES_TO_BUFFER;
    --size;
    return imagePointer;
}