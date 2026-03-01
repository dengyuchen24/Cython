#pragma once

#ifndef __DYC_VAR__
#define __DYC_VAR__

#ifndef _VCRUNTIME_H
#include <vcruntime.h>
#endif

#if _HAS_CXX17

#include <any>
#include <stdexcept>
#include <variant>
#include <vector>
#include <initializer_list>
#include <typeinfo>

namespace dyc {

	class any_type {
	private: std::any value;
	public:
		any_type() = default;
		template<typename T>
		any_type(T&& val) : value(std::forward<T>(val)) {}
		template<typename T>
		any_type& operator=(T&& val) {
			value = std::forward<T>(val); return *this;
		}
		// 获取存储的值（自动类型推导）
		template<typename T> T get() const {
			if (!has_value()) {
				throw std::runtime_error("any_type is empty");
			} try {
				return std::any_cast<T>(value);
			}
			catch (const std::bad_any_cast&) {
				throw std::runtime_error("Type mismatch in any_type::get()");
			}
		}
		// 自动类型转换操作符
		template<typename T> operator T() const { return get<T>(); }
		// 检查是否包含值
		bool has_value() const noexcept { return value.has_value(); }
		// 获取类型信息
		const std::type_info& type() const noexcept { return value.type(); }
		// 重置内容
		void reset() noexcept { value.reset(); }
		// 交换内容
		void swap(any_type& other) noexcept { value.swap(other.value); }
	};

	template <typename key_type = std::string> class var_map {
	private:
		using val_type = std::pair<key_type, any_type>;
		std::vector<val_type> key_val;
		// 查找key的索引（返回索引+1，0表示未找到）
		size_t find_key(const key_type& key) const {
			for (size_t i = 0; i < key_val.size(); ++i) {
				if (key_val[i].first == key) { return i + 1; }
			}
			return 0;
		}
		// 获取值的引用（用于修改）
		any_type& get_or_create(const key_type& key) {
			size_t index = find_key(key);
			if (index == 0) {
				// 未找到，创建新元素
				key_val.emplace_back(key, any_type{});
				return key_val.back().second;
			}
			else {
				// 找到，返回现有元素的引用
				return key_val[index - 1].second;
			}
		}
	public:
		var_map() = default;
		~var_map() = default;
		// 迭代器
		auto begin() { return key_val.begin(); }
		auto begin() const { return key_val.begin(); }
		auto end() { return key_val.end(); }
		auto end() const { return key_val.end(); }
		// 重载operator[]用于访问值
		template<typename T>
		T operator[](const key_type& key) const {
			size_t index = find_key(key);
			if (index == 0) {
				throw std::out_of_range("Key not found in var map");
			}
			return key_val[index - 1].second.get<T>();
		}
		// 重载operator[]用于设置值（返回可赋值的代理对象）
		class proxy {
		private:
			any_type& ref;
		public:
			proxy(any_type& r) : ref(r) {}
			template<typename T>
			proxy& operator=(T&& value) {
				ref = std::forward<T>(value); return *this;
			}
			// 自动类型转换
			template<typename T> operator T() const { return ref.get<T>(); }
		};

		proxy operator[](const key_type& key) { return proxy(get_or_create(key)); }
		// 插入元素
		template<typename T>
		void insert(const key_type& key, T&& value) {
			get_or_create(key) = std::forward<T>(value);
		}
		// 检查是否包含key
		bool contains(const key_type& key) const { return find_key(key) != 0; }
		// 获取元素数量
		size_t size() const noexcept { return key_val.size(); }
		// 检查是否为空
		bool empty() const noexcept { return key_val.empty(); }
		// 清空所有元素
		void clear() noexcept { key_val.clear(); }
		// 删除指定key的元素
		bool erase(const key_type& key) {
			size_t index = find_key(key);
			if (index != 0) {
				key_val.erase(key_val.begin() + (index - 1));
				return true;
			}
			return false;
		}
	};

	template<typename T>
	class MultiVar {
	private:
		std::variant<T, std::initializer_list<T>> value;

	public:
		MultiVar() : value(static_cast<T>(NULL)) {}
		MultiVar(T v) : value(v) {}
		MultiVar(std::initializer_list<T> v) : value(v) {}

		// 检查是否包含特定值
		bool operator==(T x) const {
			if (std::holds_alternative<T>(value)) {
				return std::get<T>(value) == x;
			}
			else {
				const auto& list = std::get<std::initializer_list<T>>(value);
				for (T v : list) {
					if (v == x) return true;
				}
				return false;
			}
		}

		bool operator!=(T x) const {
			return !(operator==(x));
		}

		// MultiVar 与 MultiVar 的比较
		// 优化版本 - 直接遍历，避免创建临时向量
		bool operator==(const MultiVar<T>& other) const {
			// 处理两个都是单值的情况
			if (std::holds_alternative<T>(value) && std::holds_alternative<T>(other.value)) {
				return std::get<T>(value) == std::get<T>(other.value);
			}

			// 处理至少有一个是列表的情况
			if (std::holds_alternative<T>(value)) {
				// this是单值，other是列表
				const T& singleValue = std::get<T>(value);
				const auto& otherList = std::get<std::initializer_list<T>>(other.value);
				for (const T& v : otherList) {
					if (singleValue == v) return true;
				}
			}
			else if (std::holds_alternative<T>(other.value)) {
				// this是列表，other是单值
				const T& otherSingle = std::get<T>(other.value);
				const auto& thisList = std::get<std::initializer_list<T>>(value);
				for (const T& v : thisList) {
					if (v == otherSingle) return true;
				}
			}
			else {
				// 两个都是列表
				const auto& thisList = std::get<std::initializer_list<T>>(value);
				const auto& otherList = std::get<std::initializer_list<T>>(other.value);
				for (const T& v1 : thisList) {
					for (const T& v2 : otherList) {
						if (v1 == v2) return true;
					}
				}
			}
			return false;
		}
		

		// 添加一个辅助函数来获取所有值的列表
		std::vector<T> getAllValues() const {
			std::vector<T> result;
			if (std::holds_alternative<T>(value)) {
				result.push_back(std::get<T>(value));
			}
			else {
				const auto& list = std::get<std::initializer_list<T>>(value);
				for (T v : list) {
					result.push_back(v);
				}
			}
			return result;
		}
	};
}  // namespace dyc

#endif  // _HAS_CXX17

#endif  // __DYC_VAR__