libcoring is doing benchmark and trying out some useful features and designs now.

## Timer

### skiplist and `std::pmr`

The first experiment feature would focus on the performance of timer which is a `skiplist_map` written by me without
many careful consideration and perforamance awareness.

I am trying to adopt the `pmr` available since C++17 and try to know if the use case of timer int real networking
application would benefits from the locality provided by local allocator.

The first attempt on benchmarking the raw `std::map` and `coring::skiplist_map` inserting and erasing
shows that while compared to std::map, skiplist would be slower (about 2x) most of the time. But accessing the linked
list iteratively is actually faster than `std::map` (also 2x). Consider that timer is used to store a callback handle,
thus the application won't benefit from traversal, the main focus is on the locality during searching and erasing. The
benchmark is available in file: [pmr_benchmark](../test/pmr_benchmark.cpp).

Of course, for timer, `std::map` won't work, we need `std::multimap`... Besides, the `hrtimer(essentially an rbtree)` is
a good competitor too. We also haven't try timer wheel yet but since most operations with timeout would be tied
with `IOSQE_LINK`, it's unknown if we still need to use user timer to deal with that.

There are more things to deal with timer though, since the callback(coroutine handle) resuming would consume time.

The first benchmark shows a conclusion is that there may have nothing to do between timer and locality unless you need
to traverse it inplace. I would try to extend an interface that do the coroutine resume directly on data structure
instead of copy them out. Wait to see if this helps the performance.

Conclusion on skiplist: skiplist is good for DB usage, like range query and persistence, it's easy to implement than
rb-tree in this both use case. But it's not good for inserting and erasing. Neither good for cancelling implementation
nor pop up.

Conclusion on std::pmr: it would be good for DB usage too, for timer, useless since you don't need to fully scan a timer
list.

I will tryout timer wheel then.