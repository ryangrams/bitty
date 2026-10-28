#pragma once
#ifdef M5FLAP_USB_LIBRARY
#include <esp_partition.h>
#include <esp_spi_flash.h>
#include "cartridge_store.h"

namespace flappy {
class LibraryFlash {
 public:
  bool begin() {
    partition_ = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, esp_partition_subtype_t(0x40), "library");
    if (!partition_) return false;
    const void* mapped = nullptr;
    if (esp_partition_mmap(partition_, 0, partition_->size, ESP_PARTITION_MMAP_DATA, &mapped, &handle_) != ESP_OK) return false;
    mapped_ = static_cast<const uint8_t*>(mapped);
    return true;
  }
  uint32_t size() const { return partition_ ? partition_->size : 0; }
  bool read(uint32_t offset, void* data, size_t bytes) const {
    return partition_ && offset <= size() && bytes <= size() - offset &&
        esp_partition_read(partition_, offset, data, bytes) == ESP_OK;
  }
  bool write(uint32_t offset, const void* data, size_t bytes) {
    return partition_ && offset <= size() && bytes <= size() - offset &&
        esp_partition_write(partition_, offset, data, bytes) == ESP_OK;
  }
  bool erase(uint32_t offset, size_t bytes) {
    return partition_ && offset <= size() && bytes <= size() - offset &&
        esp_partition_erase_range(partition_, offset, bytes) == ESP_OK;
  }
  const uint8_t* view(uint32_t offset, uint32_t length) const {
    return mapped_ && offset <= size() && length <= size() - offset ? mapped_ + offset : nullptr;
  }
 private:
  const esp_partition_t* partition_ = nullptr;
  esp_partition_mmap_handle_t handle_ = 0;
  const uint8_t* mapped_ = nullptr;
};
using UsbCartridges = CartridgeStore<LibraryFlash>;
} // namespace flappy
#endif
