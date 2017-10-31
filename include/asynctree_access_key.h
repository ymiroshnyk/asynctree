#pragma once

namespace AST
{

enum EnumAccessKey
{
	KEY
};

template <typename T1, typename T2 = T1, typename T3 = T2, typename T4 = T3, typename T5 = T4,
	typename T6 = T5, typename T7 = T6, typename T8 = T7, typename T9 = T8, typename T10 = T9>
	class AccessKey
{
	friend T1;
	friend T2;
	friend T3;
	friend T4;
	friend T5;
	friend T6;
	friend T7;
	friend T8;
	friend T9;
	friend T10;

	AccessKey() {}
	AccessKey(EnumAccessKey) {}
	AccessKey(const AccessKey<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10>&) {}
};

}

