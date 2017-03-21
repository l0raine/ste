//	StE
// � Shlomi Steinberg 2015-2017

#pragma once

#include <stdafx.hpp>
#include <ste_device_queues_protocol.hpp>
#include <ste_device_queue_batch.hpp>
#include <ste_device_queue_batch_multishot.hpp>
#include <ste_device_exceptions.hpp>

#include <vk_queue.hpp>
#include <vk_logical_device.hpp>
#include <ste_device_queue_command_pool.hpp>
#include <vk_command_buffers.hpp>
#include <ste_resource_pool.hpp>
#include <ste_device_sync_primitives_pools.hpp>

#include <condition_variable>
#include <mutex>
#include <future>
#include <utility>
#include <memory>
#include <vector>
#include <list>
#include <atomic>
#include <aligned_ptr.hpp>

#include <concurrent_queue.hpp>
#include <interruptible_thread.hpp>
#include <thread_pool_task.hpp>

#include <function_traits.hpp>
#include <type_traits>

namespace StE {
namespace GL {

class ste_device_queue {
public:
	using task_t = unique_thread_pool_type_erased_task<>;
	template <typename R>
	using enqueue_task_t = unique_thread_pool_task<R>;
	using queue_index_t = std::uint32_t;

private:
	using shared_fence_t = ste_device_queue_batch<void>::fence_t;

	struct shared_data_t {
		mutable std::mutex m;
		mutable std::condition_variable notifier;

		concurrent_queue<task_t> task_queue;
	};

private:
	const queue_index_t queue_index;
	const vk_queue queue;
	const ste_queue_descriptor descriptor;

	ste_device_sync_primitives_pools::shared_fence_pool_t *shared_fence_pool;

	std::list<std::unique_ptr<_detail::ste_device_queue_batch_base>> submitted_oneshot_batches;

	aligned_ptr<shared_data_t> shared_data;
	ste_resource_pool<ste_device_queue_command_pool> pool;
	std::unique_ptr<interruptible_thread> thread;

private:
	static thread_local ste_device_queue *static_device_queue_ptr;
	static thread_local const vk_queue *static_queue_ptr;
	static thread_local queue_index_t static_queue_index;

private:
	static auto& thread_device_queue() { return *static_device_queue_ptr; }

public:
	/**
	*	@brief	Returns the Vulkan queue handle for the current thread
	*			Must be called from an enqueued task.
	*/
	static auto& thread_queue() { return *static_queue_ptr; }
	/**
	*	@brief	Returns the reported queue index for the current thread
	*			Must be called from an enqueued task.
	*/
	static auto thread_queue_index() { return static_queue_index; }
	/**
	*	@brief	Checks if current thread is a queue thread
	*/
	static bool is_queue_thread() { return thread_queue_index() != 0xFFFFFFFF; }

	/**
	*	@brief	Allocates a new command batch.
	*			Must be called from an enqueued task.
	*			UserData can be any structure that holds some user data. The user data is accessible by the public user_data
	*			member in the returned batch, and is constructed with the supplied user_data_args parameter pack. The user data
	*			should be used to hold data used by the batch commands, and will be deallocated with the batch only once 
	*			processing is complete by the device.
	*/
	template <typename UserData = void, typename... UserDataArgs>
	static auto thread_allocate_batch(UserDataArgs&&... user_data_args) {
		using batch_t = ste_device_queue_batch<UserData>;
		return std::make_unique<batch_t>(thread_queue_index(),
										 thread_device_queue().pool.claim(),
										 std::make_shared<shared_fence_t>(thread_device_queue().shared_fence_pool->claim()),
										 std::forward<UserDataArgs>(user_data_args)...);
	}
	/**
	*	@brief	Allocates a new command batch.
	*			Batch should be a ste_device_queue_batch_oneshot or ste_device_queue_batch_multishot derived type.
	*			Must be called from an enqueued task.
	*/
	template <typename Batch, typename... Args>
	static auto thread_allocate_batch_custom(Args&&... custom_args) {
		return std::make_unique<Batch>(thread_queue_index(),
									   thread_device_queue().pool.claim(),
									   std::forward<Args>(custom_args)...);
	}

	/**
	*	@brief	Submits a one-shot command batch to the queue.
	*			Must be called from an enqueued task.
	*			
	*	@throws	ste_device_not_queue_thread_exception	If thread not a queue thread
	*	@throws	ste_device_exception	If batch was not created on this queue, batch's fence will be set to a ste_device_exception.
	*
	*	@param	batch				Command batch to submit
	*	@param	wait_semaphores		See vk_queue::submit
	*	@param	signal_semaphores	See vk_queue::submit
	*/
	template <typename UserData>
	static void submit_batch(std::unique_ptr<ste_device_queue_batch_oneshot<UserData>> &&batch,
							 const std::vector<std::pair<VkSemaphore, VkPipelineStageFlags>> &wait_semaphores = {},
							 const std::vector<VkSemaphore> &signal_semaphores = {}) {
		if (!is_queue_thread()) {
			throw ste_device_not_queue_thread_exception();
		}

		// Copy command buffers' handles for submission
		std::vector<vk_command_buffer> command_buffers;
		command_buffers.reserve(batch->command_buffers.size());
		for (auto &b : *batch)
			command_buffers.push_back(static_cast<vk_command_buffer>(b));

		auto& fence = batch->get_fence();

		try {
			if (batch->queue_index == thread_queue_index()) {
				// Submit finalized buffers
				thread_queue().submit(command_buffers,
									  wait_semaphores,
									  signal_semaphores,
									  &fence.get_fence());

				// And signal fence future
				fence.signal();

				batch->submitted = true;

				// Hold onto the batch, release resources only once the device is done with it
				thread_device_queue().submitted_oneshot_batches.emplace_back(std::move(batch));
			}
			else {
				assert(false && "Batch created on a different queue");
				throw ste_device_exception("Batch created on a different queue");
			}
		}
		catch (...) {
			fence.set_exception(std::current_exception());
		}
	}

	/**
	*	@brief	Submits a multi-shot command batch to the queue.
	*			Must be called from an enqueued task.
	*
	*	@throws	ste_device_not_queue_thread_exception	If thread not a queue thread
	*	@throws	ste_device_exception	If batch was not created on this queue, batch's fence will be set to a ste_device_exception.
	*
	*	@param	batch				Command batch to submit
	*	@param	wait_semaphores		See vk_queue::submit
	*	@param	signal_semaphores	See vk_queue::submit
	*	@param	fence				See vk_queue::submit
	*/
	template <typename UserData>
	static void submit_batch(const ste_device_queue_batch_multishot<UserData> &batch,
							 const std::vector<std::pair<VkSemaphore, VkPipelineStageFlags>> &wait_semaphores = {},
							 const std::vector<VkSemaphore> &signal_semaphores = {},
							 vk_fence *fence = nullptr) {
		if (!is_queue_thread()) {
			throw ste_device_not_queue_thread_exception();
		}

		// Copy command buffers' handles for submission
		std::vector<vk_command_buffer> command_buffers;
		command_buffers.reserve(batch.command_buffers.size());
		for (auto &b : batch)
			command_buffers.push_back(static_cast<vk_command_buffer>(b));

		if (batch.queue_index == thread_queue_index()) {
			// Submit finalized buffers
			thread_queue().submit(command_buffers,
									wait_semaphores,
									signal_semaphores,
									fence);
			batch.submitted = true;
		}
		else {
			assert(false && "Batch created on a different queue");
			throw ste_device_exception("Batch created on a different queue");
		}
	}

private:
	void create_worker();
	void prune_submitted_batches();

	template <typename R, typename L>
	static auto execute_in_place(L &&task,
								 typename std::enable_if<std::is_void<R>::value>::type* = nullptr) {
		std::promise<R> promise;
		auto future = promise.get_future();

		task();
		promise.set_value();

		return future;
	}
	template <typename R, typename L>
	static auto execute_in_place(L &&task,
								 typename std::enable_if<!std::is_void<R>::value>::type* = nullptr) {
		std::promise<R> promise;
		auto future = promise.get_future();
		promise.set_value(task());

		return future;
	}

public:
	ste_device_queue(const vk_logical_device &device,
					 std::uint32_t device_family_index,
					 ste_queue_descriptor descriptor,
					 queue_index_t queue_index,
					 ste_device_sync_primitives_pools::shared_fence_pool_t *shared_fence_pool)
		: queue_index(queue_index),
		queue(device, descriptor.family, device_family_index),
		descriptor(descriptor),
		shared_fence_pool(shared_fence_pool),
		pool(device, descriptor.family, 0)
	{
		// Create the queue worker thread
		create_worker();
	}
	~ste_device_queue() noexcept {
		thread->interrupt();

		do { shared_data->notifier.notify_all(); } while (!shared_data->m.try_lock());
		shared_data->m.unlock();

		thread->join();
		queue.wait_idle();
	}

	ste_device_queue(ste_device_queue &&q) = delete;
	ste_device_queue &operator=(ste_device_queue &&) = delete;
	ste_device_queue(const ste_device_queue &) = delete;
	ste_device_queue &operator=(const ste_device_queue &) = delete;

	/**
	*	@brief	Releases some pools' resources.
	*/
	void tick() {
		enqueue([=]() {
			thread_device_queue().prune_submitted_batches();
		});
	}

	/**
	*	@brief	Allocates a new command batch.
	*			UserData can be any structure that holds some user data. The user data is accessible by the public user_data
	*			member in the returned batch, and is constructed with the supplied user_data_args parameter pack. The user data
	*			should be used to hold data used by the batch commands, and will be deallocated with the batch only once 
	*			processing is complete by the device.
	*/
	template <typename UserData = void, typename... UserDataArgs>
	auto allocate_batch(UserDataArgs&&... user_data_args) {
		using batch_t = ste_device_queue_batch<UserData>;
		return std::make_unique<batch_t>(queue_index,
										 pool.claim(),
										 std::make_shared<shared_fence_t>(shared_fence_pool->claim()),
										 std::forward<UserDataArgs>(user_data_args)...);
	}
	/**
	*	@brief	Allocates a new multishot (reusable) command batch.
	*			See allocate_batch() for more information.
	*			
	*	@return	An auditor used to record the multi-shot command batch. Once recorded a call to finalize generates the final
	*			multi-shot batch that can be submitted to a queue. A batch can not be re-recorded.
	*/
	template <typename UserData = void, typename... UserDataArgs>
	auto allocate_batch_multishot(UserDataArgs&&... user_data_args) {
		using auditor_t = ste_device_queue_batch_multishot_auditor<ste_device_queue_batch_multishot<UserData>>;
		return auditor_t(queue_index,
						 pool.claim(),
						 std::forward<UserDataArgs>(user_data_args)...);
	}
	/**
	*	@brief	Allocates a new command batch.
	*			Batch should be a ste_device_queue_batch_oneshot derived type.
	*/
	template <typename Batch, typename... Args>
	auto allocate_batch_custom(Args&&... custom_args) {
		return std::make_unique<Batch>(queue_index,
									   pool.claim(),
									   std::forward<Args>(custom_args)...);
	}

	/**
	*	@brief	Enqueues a task on the queue's thread
	*
	*	@param	task	Task to enqueue
	*/
	template <typename L>
	std::future<typename function_traits<L>::result_t> enqueue(L &&task) {
		using R = typename function_traits<L>::result_t;

		static_assert(function_traits<L>::arity == 0,
					  "task must take no arguments");

		if (is_queue_thread()) {
			// Execute in place
			return execute_in_place<R>(std::move(task));
		}

		// Enqueue
		enqueue_task_t<R> f(std::forward<L>(task));
		auto future = f.get_future();

		shared_data->task_queue.push(std::move(f));
		shared_data->notifier.notify_one();

		return future;
	}

	/**
	*	@brief	Waits idly for the queue to finish processing
	*			
	*	@throws	ste_device_exception	If thread is a queue thread
	*/
	void wait_idle() const {
		if (is_queue_thread()) {
			throw ste_device_exception("Deadlock");
		}

		std::atomic_thread_fence(std::memory_order_acquire);
		while (!shared_data->task_queue.is_empty_hint()) {
			shared_data->notifier.notify_all();
			std::this_thread::sleep_for(std::chrono::milliseconds(0));
		}

		// And wait idle (for weired implementations that would signal the fence before the queue is complete)
		queue.wait_idle();
	}

	auto &device_queue() const { return queue; }
	auto &queue_descriptor() const { return descriptor; }
	auto index() const { return queue_index; }
};

}
}
