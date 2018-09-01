﻿#pragma once

#include <cstddef> // ptrdiff_t
#include "allocator.h"
#include "uninitialized.h"
#include "stl_iterator.h"

// alloc为SGI STL默认空间配置器
template<class T,class Alloc = allocator<T> >
class vector {
public:
	// 嵌套类型别名定义
	using value_type = T;
	using pointer = value_type* ;
	using iterator = value_type* ;// vector迭代器为普通指针
	using const_iterator = value_type* ;
	using r_iterator = reverse_iterator<iterator>;
	using const_r_iterator = reverse_iterator<const_iterator>;
	using reference = value_type& ;
	using const_reference = const value_type& ;
	using size_type = size_t;
	using difference_type = ptrdiff_t;

private:
	// 内存配置、批量构造函数
	using data_allocator = Alloc;

	iterator start;
	iterator finish;
	iterator end_of_stroage;// 可用空间尾部

	// 插入接口，内含扩容操作
	void insert_aux(iterator position, const value_type& value);

	// 释放本vector所占用的内存
	void deallocate() {
		if (start)
			data_allocator::deallocate(start, end_of_stroage - start);
	}

	// 获取并初始化内存区域
	void fill_initialize(size_type n, const value_type& value) {
		start = allocate_and_fill(n, value);
		finish = start + n;
		end_of_stroage = finish;
	}

	// 配置空间并填满内容(具体实现见allocator）
	iterator allocate_and_fill(size_type n, const value_type& value) {
		iterator result = data_allocator::allocate(n);
		uninitialized_fill_n(result, n, value);
		return result;
	}

public:
	// ctor && dtor
	vector() :start(nullptr), finish(nullptr), end_of_stroage(nullptr) {}
	explicit vector(size_type n) { fill_initialize(n, value_type()); }
	vector(size_type n, const value_type &value) { fill_initialize(n, value); }
	vector(const vector&);
	vector(vector&&) noexcept;

	~vector() {
		destory(start, finish);//全局函数，见allocator
		deallocate();
	}

public:
	vector& operator=(const vector v) const;
	vector& operator=(vector&&) noexcept;

public:
	//静态可写接口
	iterator begin() { return start; }
	iterator end() { return finish; }
	r_iterator rbegin() { return r_iterator(start); }
	r_iterator rend() { return r_iterator(end); }
	reference operator[](const size_type n) { return *(start + n); }
	reference front() { return *begin(); }
	reference back() { return *(end() - 1); }

public:
	//静态只读接口
	const_iterator begin() const { return start; }
	const_iterator end() const { return end; }
	const_iterator cbegin() const { return start; }
	const_iterator cend() const { return end; }
	const_r_iterator crbegin() const { return const_r_iterator(start); }
	const_r_iterator crend() const { return const_r_iterator(end); }
	const_reference operator[](const size_type n) const  { return *(start + n); }
	size_type size() const { return static_cast<size_type>(finish - start); }
	size_type capacity() const { return static_cast<size_type>(end_of_stroage - start); }
	bool empty() const { return start == finish; }

public:
	//动态接口
	void push_back(const value_type&value) {
		if (finish != end_of_stroage) {
			construct(finish, value);//全局函数
			++finish;
		}
		else
			insert_aux(end(), value);
	}

	void pop_back() {
		--finish;
		destory(finish);
	}

	iterator erase(iterator first, iterator last) {
		iterator i = copy(last, finish, first);
		destory(i, finish);//此时i即等价于new_finish
		finish -= (last - first);
		return first;
	}

	iterator erase(iterator position) {
		if (position + 1 != end()) //除却尾端节点外均需复制
			copy(position + 1, finish, position);
		pop_back();
		return position;
	}

	void resize(size_type new_size, const value_type& value) {
		if (new_size < size()) {
			erase(begin() + new_size, end());
		}
		else
			insert(end(), new_size - size(), value);
	}

	void resize(size_type new_size) {
		resize(new_size, value_type());
	}

	void insert(iterator position, size_type n, const value_type& value);
};

template<class T, class Alloc>
void vector<T, Alloc>::insert_aux(iterator position, const value_type& value){
	if (finish != end_of_stroage) {
		//当前存在备用空间
		construct(finish, *(finish - 1));//以最后一个元素为初值构造元素于finish
		++finish;
		value_type value_copy = value;//STL copy in copy out
		// copy_backward needs _SCL_SECURE_NO_WARNINGS
		std::copy_backward(position, finish - 2, finish - 1);//将[pos,finish-2)copy至finish-1处（finish-1为目的终点）
		*position = value_copy;
	}
	else {
		//扩容
		const size_type old_size = size();
		const size_type new_size = old_size ? 2 * old_size : 1;//2倍大小
		iterator new_start = data_allocator::allocate(new_size);
		iterator new_finish = new_start;
		try{
			new_finish = uninitialized_copy(start,position,new_start);//复制前半段
			construct(new_finish, value);
			++new_finish;
			new_finish = uninitialized_copy(position, finish, new_finish);//复制后半段
		}
		catch(std::exception&){
			//commit or rollback
			destory(new_start, new_finish);
			data_allocator::deallocate(new_start, new_size);
			throw;
		}
		//释放原有vector
		destory(begin(), end());
		deallocate();
		//调整迭代器指向新vector
		start = new_start;
		finish = new_finish;
		end_of_stroage = new_start + new_size;
	}
}

template<class T, class Alloc>
void vector<T, Alloc>::insert(iterator position, size_type n, const value_type & value){
	if (n) {
		if (static_cast<size_type>(end_of_stroage - finish) >= n) {
			//备用空间充足
			value_type value_copy = value;
			const size_type elems_after = finish - position;
			iterator old_finish = finish;
			if (elems_after > n) {
				//插入点后元素个数m>=插入元素个数n
				unitialized_copy(finish - n, finish, finish);//先复制后n个元素
				finish += n;
				// copy_backward needs _SCL_SECURE_NO_WARNINGS
				std::copy_backward(position, old_finish - n, old_finish);//复制m-n个元素
				fill(position, position + n, value_copy);
			}
			else {
				unitialized_fill_n(finish, n - eles_after, value_copy);//以m-n个value填充末尾
				finish += n - elems_after;
				unitialized_copy(position, old_finish, finish);//将m个填充至最末尾
				finish += elems_after;
				fill(position, old_finish,value_copy);//补足m
			}
		}
		else {
			//需要扩容
			const size_type old_size = size();
			const size_type new_size = oldsize + max(oldsize, n);
			iterator new_start = data_allocator::allocate(new_size);
			iterator new_finish = newstart;
			try{
				new_finish = unitialized_copy(start,position,new_start);
				new_finish = unitialized_fill_n(new_finish, n, value);
				new_finish = unitialized_copy(position, finish, new_finish);
			}
			catch(std::exception&){
				destory(new_start, new_finish);
				data_allocator::deallocate(new_start, new_size);
				throw;
			}
			destory(begin(), end());
			deallocate();
			start = new_start;
			finish = new_finish;
			end_of_stroage = new_start + new_size;
		}
	}
}
