namespace Collector {
	struct Allocation {
		void * ptr;
		bool mark;
	};
	List<Allocation> allocations;
	void init()
	{
		allocations.alloc();
	}
	void * alloc(size_t size)
	{
		Allocation allocation;
		allocation.ptr = malloc(size);
		allocation.mark = false;
		allocations.push(allocation);
		return allocation.ptr;
	}
};
