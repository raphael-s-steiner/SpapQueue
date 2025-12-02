#pragma once

#include <iterator>

#include "RingBuffer/RingBuffer.hpp"
#include "SpapQueueDeclaration.hpp"

namespace spapq {

template <typename GlobalQType, typename LocalQType, std::size_t numPorts>
class WorkerResource {
    template <typename, typename, std::size_t>
    friend class WorkerResource;
    template <typename,
              std::size_t workers,
              std::size_t channels,
              QNetwork<workers, channels>,
              template <class, class, std::size_t> class,
              typename>
    friend class SpapQueue;

  public:
    using value_type = GlobalQType::value_type;

  private:
    GlobalQType &globalQueue_;
    typename std::array<value_type, GlobalQType::netw_.maxBatchSize()>::iterator bufferPointer_;
    typename std::array<std::size_t, maxTableSize(GlobalQType::netw_)>::const_iterator channelPointer_;
    const typename std::array<std::size_t, maxTableSize(GlobalQType::netw_)>::const_iterator
        channelTableEndPointer_;
    std::array<value_type, GlobalQType::netw_.maxBatchSize()> outBuffer_;
    const std::array<std::size_t, maxTableSize(GlobalQType::netw_)> channelIndices_;
    std::array<RingBuffer<value_type, GlobalQType::netw_.bufferSize_>, numPorts> inPorts_;
    LocalQType queue_;

    [[nodiscard("Push may fail when queue is full")]] inline bool pushOutBuffer();
    inline void pushOutBufferSelf(
        const typename std::array<value_type, GlobalQType::netw_.maxBatchSize()>::iterator fromPointer);

    inline void enqueueInChannels();
    inline void processElement(const value_type &val) = 0;

    [[nodiscard("Push may fail when queue is full")]] inline bool push(const value_type &val,
                                                                       std::size_t port);
    [[nodiscard("Push may fail when queue is full")]] inline bool push(value_type &&val, std::size_t port);
    template <class InputIt>
    [[nodiscard("Push may fail when queue is full")]] inline bool push(InputIt first,
                                                                       InputIt last,
                                                                       std::size_t port);

  protected:
    inline void enqueueGlobal(const value_type &val);

  public:
    template <std::size_t channelIndicesLength>
    constexpr WorkerResource(GlobalQType &globalQueue,
                             const std::array<std::size_t, channelIndicesLength> &channelIndices);
    constexpr WorkerResource(GlobalQType &globalQueue, std::size_t workerId);
    WorkerResource(const WorkerResource &other) = delete;
    WorkerResource(WorkerResource &&other) = delete;
    WorkerResource &operator=(const WorkerResource &other) = delete;
    WorkerResource &operator=(WorkerResource &&other) = delete;
    ~WorkerResource() = default;

    inline void run();
};

}    // end namespace spapq
