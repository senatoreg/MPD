// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "util/IntrusiveList.hxx"
#include "util/SortList.hxx"

#include <gtest/gtest.h>

#include <string>

namespace {

template<IntrusiveHookMode mode>
struct CharItem final : IntrusiveListHook<mode> {
	char ch;

	constexpr CharItem(char _ch) noexcept:ch(_ch) {}
};

template<IntrusiveHookMode mode>
static std::string
ToString(const IntrusiveList<CharItem<mode>> &list,
	 typename IntrusiveList<CharItem<mode>>::const_iterator it,
	 std::size_t n) noexcept
{
	std::string result;
	for (std::size_t i = 0; i < n; ++i, ++it)
		result.push_back(it == list.end() ? '_' : it->ch);
	return result;
}

template<IntrusiveHookMode mode>
static std::string
ToStringReverse(const IntrusiveList<CharItem<mode>> &list,
		typename IntrusiveList<CharItem<mode>>::const_iterator it,
		std::size_t n) noexcept
{
	std::string result;
	for (std::size_t i = 0; i < n; ++i, --it)
		result.push_back(it == list.end() ? '_' : it->ch);
	return result;
}

} // anonymous namespace

TEST(IntrusiveList, Basic)
{
	using Item = CharItem<IntrusiveHookMode::NORMAL>;

	Item items[]{'a', 'b', 'c'};

	IntrusiveList<Item> list;
	for (auto &i : items)
		list.push_back(i);

	ASSERT_EQ(ToString(list, list.begin(), 5), "abc_a");
	ASSERT_EQ(ToStringReverse(list, list.begin(), 5), "a_cba");

	items[1].unlink();

	ASSERT_EQ(ToString(list, list.begin(), 4), "ac_a");
	ASSERT_EQ(ToStringReverse(list, list.begin(), 4), "a_ca");

	IntrusiveList<Item> other_list;
	Item other_items[]{'d', 'e', 'f', 'g'};
	for (auto &i : other_items)
		other_list.push_back(i);

	list.splice(std::next(list.begin()), other_list,
		    other_list.iterator_to(other_items[1]),
		    other_list.iterator_to(other_items[3]), 2);

	ASSERT_EQ(ToString(other_list, other_list.begin(), 4), "dg_d");
	ASSERT_EQ(ToStringReverse(other_list, other_list.begin(), 4), "d_gd");

	ASSERT_EQ(ToString(list, list.begin(), 6), "aefc_a");
	ASSERT_EQ(ToStringReverse(list, list.begin(), 6), "a_cfea");
}

TEST(IntrusiveList, Track)
{
	using Item = CharItem<IntrusiveHookMode::TRACK>;

	Item items[]{'a', 'b', 'c'};

	for (const auto &i : items)
		ASSERT_FALSE(i.is_linked());

	IntrusiveList<Item> list;

	list.push_back(items[1]);
	list.push_back(items[2]);
	list.push_front(items[0]);

	for (const auto &i : items)
		ASSERT_TRUE(i.is_linked());

	ASSERT_EQ(ToString(list, list.begin(), 5), "abc_a");
	ASSERT_EQ(ToStringReverse(list, list.begin(), 5), "a_cba");

	items[1].unlink();

	ASSERT_TRUE(items[0].is_linked());
	ASSERT_FALSE(items[1].is_linked());
	ASSERT_TRUE(items[2].is_linked());

	ASSERT_EQ(ToString(list, list.begin(), 4), "ac_a");
	ASSERT_EQ(ToStringReverse(list, list.begin(), 4), "a_ca");

	list.erase(list.iterator_to(items[0]));

	ASSERT_FALSE(items[0].is_linked());
	ASSERT_FALSE(items[1].is_linked());
	ASSERT_TRUE(items[2].is_linked());

	ASSERT_EQ(ToString(list, list.begin(), 3), "c_c");
	ASSERT_EQ(ToStringReverse(list, list.begin(), 3), "c_c");

	list.clear();

	ASSERT_FALSE(items[0].is_linked());
	ASSERT_FALSE(items[1].is_linked());
	ASSERT_FALSE(items[2].is_linked());

	ASSERT_EQ(ToString(list, list.begin(), 2), "__");
	ASSERT_EQ(ToStringReverse(list, list.begin(), 2), "__");

	{
		IntrusiveList<Item> list2;
		list2.push_back(items[0]);
		ASSERT_TRUE(items[0].is_linked());
	}

	ASSERT_FALSE(items[0].is_linked());
}

TEST(IntrusiveList, AutoUnlink)
{
	using Item = CharItem<IntrusiveHookMode::AUTO_UNLINK>;

	Item a{'a'};
	ASSERT_FALSE(a.is_linked());

	IntrusiveList<Item> list;

	Item b{'b'};
	ASSERT_FALSE(b.is_linked());

	{
		Item c{'c'};

		list.push_back(a);
		list.push_back(b);
		list.push_back(c);

		ASSERT_TRUE(a.is_linked());
		ASSERT_TRUE(b.is_linked());
		ASSERT_TRUE(c.is_linked());

		ASSERT_EQ(ToString(list, list.begin(), 5), "abc_a");
	}

	ASSERT_EQ(ToString(list, list.begin(), 5), "ab_ab");

	ASSERT_TRUE(a.is_linked());
	ASSERT_TRUE(b.is_linked());
}

TEST(IntrusiveList, Merge)
{
	using Item = CharItem<IntrusiveHookMode::NORMAL>;

	const auto predicate = [](const Item &a, const Item &b){
		return a.ch < b.ch;
	};

	Item items[]{'c', 'k', 'u'};

	IntrusiveList<Item> list;
	for (auto &i : items)
		list.push_back(i);

	IntrusiveList<Item> other_list;
	Item other_items[]{'a', 'b', 'g', 'm', 'n', 'x', 'y', 'z'};
	for (auto &i : other_items)
		other_list.push_back(i);

	MergeList(list, other_list, predicate);

	ASSERT_EQ(ToString(list, list.begin(), 13), "abcgkmnuxyz_a");
	ASSERT_TRUE(other_list.empty());

	Item more_items[]{'a', 'o', 'p', 'q'};
	for (auto &i : more_items)
		other_list.push_back(i);

	MergeList(list, other_list, predicate);

	ASSERT_EQ(ToString(list, list.begin(), 17), "aabcgkmnopquxyz_a");
	ASSERT_EQ(&*list.begin(), &other_items[0]);
	ASSERT_EQ(&*std::next(list.begin()), &more_items[0]);
}

TEST(IntrusiveList, Sort)
{
	using Item = CharItem<IntrusiveHookMode::NORMAL>;

	const auto predicate = [](const Item &a, const Item &b){
		return a.ch < b.ch;
	};

	Item items[]{'z', 'a', 'b', 'q', 'b', 'c', 't', 'm', 'y'};

	IntrusiveList<Item> list;
	SortList(list, predicate);
	ASSERT_EQ(ToString(list, list.begin(), 2), "__");

	list.push_back(items[0]);
	SortList(list, predicate);
	ASSERT_EQ(ToString(list, list.begin(), 3), "z_z");

	list.push_back(items[1]);
	SortList(list, predicate);
	ASSERT_EQ(ToString(list, list.begin(), 4), "az_a");
	SortList(list, predicate);
	ASSERT_EQ(ToString(list, list.begin(), 4), "az_a");

	list.clear();
	for (auto &i : items)
		list.push_back(i);

	SortList(list, predicate);
	ASSERT_EQ(ToString(list, list.begin(), 11), "abbcmqtyz_a");
	ASSERT_EQ(&*std::next(list.begin(), 1), &items[2]);
	ASSERT_EQ(&*std::next(list.begin(), 2), &items[4]);
}

#include "util/IntrusiveSortedList.hxx"

TEST(IntrusiveSortedList, Basic)
{
	using Item = CharItem<IntrusiveHookMode::NORMAL>;

	struct Compare {
		constexpr bool operator()(const Item &a, const Item &b) noexcept {
			return a.ch < b.ch;
		}
	};

	Item items[]{'z', 'a', 'b', 'q', 'b', 'c', 't', 'm', 'y'};

	IntrusiveSortedList<Item, Compare> list;
	ASSERT_EQ(ToString(list, list.begin(), 2), "__");

	list.insert(items[0]);
	ASSERT_EQ(ToString(list, list.begin(), 3), "z_z");

	list.insert(items[1]);
	ASSERT_EQ(ToString(list, list.begin(), 4), "az_a");

	list.insert(items[2]);
	ASSERT_EQ(ToString(list, list.begin(), 5), "abz_a");

	list.insert(items[3]);
	ASSERT_EQ(ToString(list, list.begin(), 6), "abqz_a");

	list.insert(items[4]);
	ASSERT_EQ(ToString(list, list.begin(), 7), "abbqz_a");

	list.insert(items[5]);
	ASSERT_EQ(ToString(list, list.begin(), 8), "abbcqz_a");

	list.insert(items[6]);
	ASSERT_EQ(ToString(list, list.begin(), 9), "abbcqtz_a");

	list.insert(items[7]);
	ASSERT_EQ(ToString(list, list.begin(), 10), "abbcmqtz_a");

	list.insert(items[8]);
	ASSERT_EQ(ToString(list, list.begin(), 11), "abbcmqtyz_a");
}
