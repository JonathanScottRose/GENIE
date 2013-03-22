#pragma once

template<class T>
static void at_delete_all(T& container)
{
	T::iterator i = container.begin();
	T::iterator end = container.end();

	for (auto i : container)
	{
		delete i;
	}
}

template<class T>
static void at_delete_all_2(T& container)
{
	for (auto i : container)
	{
		delete i.second;
	}
}
