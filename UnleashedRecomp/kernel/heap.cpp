#include <stdafx.h>
#include "heap.h"
#include "memory.h"
#include "function.h"

constexpr size_t RESERVED_BEGIN = 0x7FEA0000;
constexpr size_t RESERVED_END = 0xA0000000;

#ifdef UNLEASHED_RECOMP_IOS
// Freed guest memory stays resident unless its pages are given back to the OS, and every dirty page counts against the
// memory limit of an iOS app. o1heap doesn't reuse freed memory in address order, so the set of touched pages keeps
// growing every time a stage gets loaded until iOS terminates the game. Only large blocks are worth a system call.
constexpr size_t DISCARD_THRESHOLD = 64 * 1024;

static void DiscardPages(void* address, size_t size)
{
    // Replacing the pages with fresh anonymous memory releases them immediately.
    // MADV_DONTNEED only deactivates them on Darwin, which doesn't reduce the app's footprint.
    void* result = mmap(address, size, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_PRIVATE | MAP_ANON, -1, 0);
    assert(result == address);
    (void)result;
}
#endif

static void FreeFragment(O1HeapInstance* heap, void* ptr)
{
#ifdef UNLEASHED_RECOMP_IOS
    if (ptr == nullptr)
        return;

    static const uintptr_t s_pageSize = uintptr_t(sysconf(_SC_PAGESIZE));

    // Relies on the fragment header in o1heap.c, like Heap::Size.
    uintptr_t blockBegin = uintptr_t(ptr) - O1HEAP_ALIGNMENT;
    uintptr_t blockEnd = blockBegin + *((size_t*)ptr - 2);

    void* unusedBegin;
    void* unusedEnd;
    o1heapFreeAndGetUnusedRange(heap, ptr, &unusedBegin, &unusedEnd);

    // Only discard the pages overlapping this block that are now entirely free. Pages that only
    // overlap free neighbors were already considered when those neighbors got freed.
    uintptr_t begin = std::max(blockBegin & ~(s_pageSize - 1), (uintptr_t(unusedBegin) + s_pageSize - 1) & ~(s_pageSize - 1));
    uintptr_t end = std::min((blockEnd + s_pageSize - 1) & ~(s_pageSize - 1), uintptr_t(unusedEnd) & ~(s_pageSize - 1));

    if (end > begin && (end - begin) >= DISCARD_THRESHOLD)
        DiscardPages((void*)begin, end - begin);
#else
    o1heapFree(heap, ptr);
#endif
}

void Heap::Init()
{
    heap = o1heapInit(g_memory.Translate(0x20000), RESERVED_BEGIN - 0x20000);
    physicalHeap = o1heapInit(g_memory.Translate(RESERVED_END), 0x100000000 - RESERVED_END);
}

void* Heap::Alloc(size_t size)
{
    std::lock_guard lock(mutex);

    return o1heapAllocate(heap, std::max<size_t>(1, size));
}

void* Heap::AllocPhysical(size_t size, size_t alignment)
{
    size = std::max<size_t>(1, size);
    alignment = alignment == 0 ? 0x1000 : std::max<size_t>(16, alignment);

    std::lock_guard lock(physicalMutex);

    void* ptr = o1heapAllocate(physicalHeap, size + alignment);
    size_t aligned = ((size_t)ptr + alignment) & ~(alignment - 1);

    *((void**)aligned - 1) = ptr;
    *((size_t*)aligned - 2) = size + O1HEAP_ALIGNMENT;

    return (void*)aligned;
}

void Heap::Free(void* ptr)
{
    if (ptr >= physicalHeap)
    {
        std::lock_guard lock(physicalMutex);
        FreeFragment(physicalHeap, *((void**)ptr - 1));
    }
    else
    {
        std::lock_guard lock(mutex);
        FreeFragment(heap, ptr);
    }
}

size_t Heap::Size(void* ptr)
{
    if (ptr)
        return *((size_t*)ptr - 2) - O1HEAP_ALIGNMENT; // relies on fragment header in o1heap.c

    return 0;
}

uint32_t RtlAllocateHeap(uint32_t heapHandle, uint32_t flags, uint32_t size)
{
    void* ptr = g_userHeap.Alloc(size);
    if ((flags & 0x8) != 0)
        memset(ptr, 0, size);

    assert(ptr);
    return g_memory.MapVirtual(ptr);
}

uint32_t RtlReAllocateHeap(uint32_t heapHandle, uint32_t flags, uint32_t memoryPointer, uint32_t size)
{
    void* ptr = g_userHeap.Alloc(size);
    if ((flags & 0x8) != 0)
        memset(ptr, 0, size);

    if (memoryPointer != 0)
    {
        void* oldPtr = g_memory.Translate(memoryPointer);
        memcpy(ptr, oldPtr, std::min<size_t>(size, g_userHeap.Size(oldPtr)));
        g_userHeap.Free(oldPtr);
    }

    assert(ptr);
    return g_memory.MapVirtual(ptr);
}

uint32_t RtlFreeHeap(uint32_t heapHandle, uint32_t flags, uint32_t memoryPointer)
{
    if (memoryPointer != NULL)
        g_userHeap.Free(g_memory.Translate(memoryPointer));

    return true;
}

uint32_t RtlSizeHeap(uint32_t heapHandle, uint32_t flags, uint32_t memoryPointer)
{
    if (memoryPointer != NULL)
        return (uint32_t)g_userHeap.Size(g_memory.Translate(memoryPointer));

    return 0;
}

uint32_t XAllocMem(uint32_t size, uint32_t flags)
{
    void* ptr = (flags & 0x80000000) != 0 ?
        g_userHeap.AllocPhysical(size, (1ull << ((flags >> 24) & 0xF))) :
        g_userHeap.Alloc(size);

    if ((flags & 0x40000000) != 0)
        memset(ptr, 0, size);

    assert(ptr);
    return g_memory.MapVirtual(ptr);
}

void XFreeMem(uint32_t baseAddress, uint32_t flags)
{
    if (baseAddress != NULL)
        g_userHeap.Free(g_memory.Translate(baseAddress));
}

GUEST_FUNCTION_STUB(sub_82BD7788); // HeapCreate
GUEST_FUNCTION_STUB(sub_82BD9250); // HeapDestroy

GUEST_FUNCTION_HOOK(sub_82BD7D30, RtlAllocateHeap);
GUEST_FUNCTION_HOOK(sub_82BD8600, RtlFreeHeap);
GUEST_FUNCTION_HOOK(sub_82BD88F0, RtlReAllocateHeap);
GUEST_FUNCTION_HOOK(sub_82BD6FD0, RtlSizeHeap);

GUEST_FUNCTION_HOOK(sub_831CC9C8, XAllocMem);
GUEST_FUNCTION_HOOK(sub_831CCA60, XFreeMem);
