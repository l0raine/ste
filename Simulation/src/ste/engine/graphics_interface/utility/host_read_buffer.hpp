//	StE
// � Shlomi Steinberg 2015-2017

#pragma once

#include <stdafx.hpp>
#include <ste_context.hpp>
#include <ste_queue_type.hpp>

#include <buffer_usage.hpp>
#include <device_buffer.hpp>
#include <device_buffer_sparse.hpp>
#include <device_resource_allocation_policy.hpp>

#include <command_recorder.hpp>
#include <pipeline_barrier.hpp>
#include <cmd_pipeline_barrier.hpp>
#include <cmd_copy_buffer.hpp>

#include <array.hpp>
#include <vector.hpp>
#include <stable_vector.hpp>

#include <cstring>
#include <lib/vector.hpp>

namespace ste {
namespace gl {

namespace _internal {

template <typename T>
auto host_read_buffer(const ste_context &ctx,
					  const device_buffer_base &buffer,
					  std::size_t copy_count,
					  std::size_t offset = 0) {
	using staging_buffer_t = device_buffer<T, device_resource_allocation_policy_host_visible>;

	// Select queue
	constexpr ste_queue_type queue_type = ste_queue_type::data_transfer_queue;
	const auto queue_selector = ste_queue_selector<ste_queue_selector_policy_flexible>(queue_type);
	auto &q = ctx.device().select_queue(queue_selector);

	// Staging buffer
	staging_buffer_t staging_buffer(ctx,
									copy_count,
									buffer_usage::transfer_dst,
									"host_read_buffer staging buffer");
	auto staging_ptr = staging_buffer.get_underlying_memory().template mmap<T>(0, copy_count);

	// Create copy command
	const VkBufferCopy range = { offset * sizeof(T), 0, copy_count * sizeof(T) };
	auto cpy_cmd = cmd_copy_buffer(buffer,
								   staging_buffer,
								   { range });

	// Create a batch
	auto batch = q.allocate_batch<staging_buffer_t>(std::move(staging_buffer));
	auto fence = batch->get_fence_ptr();

	// Enqueue on a transfer queue
	q.enqueue([batch = std::move(batch), cpy_cmd = std::move(cpy_cmd)]() mutable {
		auto& command_buffer = batch->acquire_command_buffer();

		// Record and submit a one-time batch
		{
			auto recorder = command_buffer.record();

			// Copy to staging buffer
			recorder
				<< cpy_cmd
				<< cmd_pipeline_barrier(pipeline_barrier(pipeline_stage::transfer,
														 pipeline_stage::host,
														 buffer_memory_barrier(batch->user_data(),
																			   access_flags::transfer_write,
																			   access_flags::host_read)));
		}
		ste_device_queue::submit_batch(std::move(batch));
	});

	// Return future that reads from the device buffer
	return ctx.engine().task_scheduler().schedule_now([copy_count, staging_ptr = std::move(staging_ptr), fence = std::move(fence)]() {
		// Create return buffer
		lib::vector<T> dst;
		dst.resize(copy_count);

		// Wait for device completion 
		(*fence)->get_wait();

		// Invalidate caches
		staging_ptr->invalidate_ranges({ vk::vk_mapped_memory_range{ 0, copy_count } });
		// Copy from staging
		std::memcpy(dst.data(), staging_ptr->get_mapped_ptr(), static_cast<std::size_t>(copy_count * sizeof(T)));

		return dst;
	});
}

}

/*
*	@brief	Reads data from a device buffer to host.
*			Asynchrnous call, returns a future. Buffer must have transfer_src usage flag.
*
*	@param	ctx			Context
*	@param	buffer		Device buffer to read from
*	@param	elements	Elements count to read
*	@param	offset		Offset in source buffer to start reading from
*/
template <typename T, class allocation_policy>
auto host_read_buffer(const ste_context &ctx,
					  const device_buffer<T, allocation_policy> &buffer,
					  std::size_t elements = std::numeric_limits<std::size_t>::max(),
					  std::size_t offset = 0) {
	const auto buffer_size = buffer.get().get_elements_count() - offset;
	const auto copy_count = std::min(elements, buffer_size);

	return _internal::host_read_buffer<T>(ctx,
										  buffer,
										  copy_count,
										  offset);
}

/*
*	@brief	Reads data from a device buffer to host.
*			Asynchrnous call, returns a future. Buffer must have transfer_src usage flag.
*
*	@param	ctx			Context
*	@param	buffer		Device buffer to read from
*	@param	elements	Elements count to read
*	@param	offset		Offset in source buffer to start reading from
*/
template <typename T, class allocation_policy>
auto host_read_buffer(const ste_context &ctx,
					  const device_buffer_sparse<T, allocation_policy> &buffer,
					  std::size_t elements,
					  std::size_t offset = 0) {
	const auto buffer_size = buffer.get().get_elements_count() - offset;
	const auto copy_count = std::min(elements, buffer_size);

	return _internal::host_read_buffer<T>(ctx,
										  buffer,
										  copy_count,
										  offset);
}

/*
*	@brief	Reads data from a device buffer to host.
*			Asynchrnous call, returns a future. Buffer must have transfer_src usage flag.
*
*	@param	ctx			Context
*	@param	buffer		Device buffer to read from
*	@param	elements	Elements count to read
*	@param	offset		Offset in source buffer to start reading from
*/
template <typename T>
auto host_read_buffer(const ste_context &ctx,
					  const array<T> &buffer,
					  std::size_t elements = std::numeric_limits<std::size_t>::max(),
					  std::size_t offset = 0) {
	return host_read_buffer(ctx,
							buffer.get(),
							elements,
							offset);
}

/*
*	@brief	Reads data from a device buffer to host.
*			Asynchrnous call, returns a future. Buffer must have transfer_src usage flag.
*
*	@param	ctx			Context
*	@param	buffer		Device buffer to read from
*	@param	elements	Elements count to read
*	@param	offset		Offset in source buffer to start reading from
*/
template <typename T, std::uint64_t max_sparse_size>
auto host_read_buffer(const ste_context &ctx,
					  const vector<T, max_sparse_size> &buffer,
					  std::size_t elements = std::numeric_limits<std::size_t>::max(),
					  std::size_t offset = 0) {
	const auto buffer_size = buffer.size() - offset;
	elements = std::min(elements, buffer_size);

	return host_read_buffer(ctx,
							buffer.get(),
							elements,
							offset);
}

/*
*	@brief	Reads data from a device buffer to host.
*			Asynchrnous call, returns a future. Buffer must have transfer_src usage flag.
*
*	@param	ctx			Context
*	@param	buffer		Device buffer to read from
*	@param	elements	Elements count to read
*	@param	offset		Offset in source buffer to start reading from
*/
template <typename T, std::uint64_t max_sparse_size>
auto host_read_buffer(const ste_context &ctx,
					  const stable_vector<T, max_sparse_size> &buffer,
					  std::size_t elements = std::numeric_limits<std::size_t>::max(),
					  std::size_t offset = 0) {
	const auto buffer_size = buffer.size() - offset;
	elements = std::min(elements, buffer_size);

	return host_read_buffer(ctx,
							buffer.get(),
							elements,
							offset);
}

}
}
