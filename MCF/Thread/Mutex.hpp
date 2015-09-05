// 这个文件是 MCF 的一部分。
// 有关具体授权说明，请参阅 MCFLicense.txt。
// Copyleft 2013 - 2015, LH_Mouse. All wrongs reserved.

#ifndef MCF_THREAD_USER_MUTEX_HPP_
#define MCF_THREAD_USER_MUTEX_HPP_

#include "../Utilities/Noncopyable.hpp"
#include "UniqueLockTemplate.hpp"
#include "Atomic.hpp"
#include "Semaphore.hpp"
#include <cstddef>

namespace MCF {

// 由一个线程锁定的互斥锁可以由另一个线程解锁。

class Mutex : NONCOPYABLE {
private:
	struct $QueueNode;

public:
	using UniqueLock = UniqueLockTemplate<Mutex>;

private:
	Atomic<std::size_t> $uSpinCount;

	Atomic<std::size_t> $uLockingThreadId;
	Semaphore $vSemaphore;
	Atomic<$QueueNode *> $pQueueHead;

public:
	explicit Mutex(std::size_t uSpinCount = 0x100);

private:
	bool $IsQueueEmpty() const noexcept;
	$QueueNode *$LockQueue() noexcept;
	void $UnlockQueue($QueueNode *pQueueHead) noexcept;

public:
	std::size_t GetSpinCount() const noexcept;
	void SetSpinCount(std::size_t uSpinCount) noexcept;

	bool Try() noexcept;
	void Lock() noexcept;
	void Unlock() noexcept;

	UniqueLock TryLock() noexcept {
		UniqueLock vLock(*this, false);
		vLock.Try();
		return vLock;
	}
	UniqueLock GetLock() noexcept {
		return UniqueLock(*this);
	}
};

}

#endif
