/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2025 Electronic Arts Inc.
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
*/

#pragma once

#include <cstring>

// GeneralsX @feature OpenAI 23/09/2026 Keep non-GUI binding rules independently testable.
namespace KeyBindingRules
{
inline bool AreLogicalPartners(const char *left, const char *right)
{
	if (!left || !right)
		return false;
	if (std::strncmp(left, "BEGIN_", 6) == 0 && std::strncmp(right, "END_", 4) == 0)
		return std::strcmp(left + 6, right + 4) == 0;
	if (std::strncmp(left, "END_", 4) == 0 && std::strncmp(right, "BEGIN_", 6) == 0)
		return std::strcmp(left + 4, right + 6) == 0;
	return false;
}

inline bool BindingsConflict(int leftKey, int leftModifiers, unsigned int leftContexts,
	int rightKey, int rightModifiers, unsigned int rightContexts, bool sameLogicalAction)
{
	return !sameLogicalAction && leftKey == rightKey && leftModifiers == rightModifiers &&
		(leftContexts & rightContexts) != 0;
}

inline bool SplitOverride(const char *value, const char *&separator)
{
	separator = value ? std::strchr(value, ',') : nullptr;
	return separator && separator != value && separator[1] != '\0' && std::strchr(separator + 1, ',') == nullptr;
}

inline int NormalizeModifiersForKey(int key, int noneKey, int modifiers)
{
	return key == noneKey ? 0 : modifiers;
}
}

class HeldCameraPanState
{
public:
	HeldCameraPanState() { clear(); }
	void clear() { for (int i = 0; i < 4; ++i) m_directions[i] = false; }
	void set(int direction, bool pressed) { if (direction >= 0 && direction < 4) m_directions[direction] = pressed; }
	bool get(int direction) const { return direction >= 0 && direction < 4 && m_directions[direction]; }
	bool any() const { return m_directions[0] || m_directions[1] || m_directions[2] || m_directions[3]; }
	int horizontal() const { return (m_directions[3] ? 1 : 0) - (m_directions[2] ? 1 : 0); }
	int vertical() const { return (m_directions[1] ? 1 : 0) - (m_directions[0] ? 1 : 0); }

private:
	bool m_directions[4];
};

// GeneralsX @feature OpenAI 24/09/2026 Track which physical key actually started each pan action.
class ActiveCameraPanBindings
{
public:
	ActiveCameraPanBindings() { clear(); }
	void clear()
	{
		for (int i = 0; i < 4; ++i)
		{
			m_active[i] = false;
			m_keys[i] = 0;
		}
	}
	void activate(int direction, int key)
	{
		if (direction >= 0 && direction < 4)
		{
			m_active[direction] = true;
			m_keys[direction] = key;
		}
	}
	unsigned int releasePhysicalKey(int key)
	{
		unsigned int releasedDirections = 0;
		for (int i = 0; i < 4; ++i)
		{
			if (m_active[i] && m_keys[i] == key)
			{
				m_active[i] = false;
				releasedDirections |= 1U << i;
			}
		}
		return releasedDirections;
	}
	bool isActive(int direction) const { return direction >= 0 && direction < 4 && m_active[direction]; }

private:
	bool m_active[4];
	int m_keys[4];
};
