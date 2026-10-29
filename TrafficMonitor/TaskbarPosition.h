#pragma once

namespace TaskbarPosition
{
struct Placement
{
    int x{}, y{}, width{}, height{};
    bool operator==(const Placement &other) const
    {
        return x == other.x && y == other.y && width == other.width && height == other.height;
    }
};

class PlacementCache
{
  public:
    bool NeedsMove(const Placement &target, bool force) const
    {
        return force || !m_valid || !(target == m_last);
    }
    void Commit(const Placement &target, bool moved)
    {
        m_valid = moved;
        if (moved)
            m_last = target;
    }
    void Reset()
    {
        m_valid = false;
    }

  private:
    Placement m_last{};
    bool m_valid{};
};

// Coordinates are taskbar-client pixels. Explicit widget avoidance is opt-in.
constexpr int LeftEdge(bool avoid_widgets, bool widgets_shown, int widget_width_px)
{
    return avoid_widgets && widgets_shown && widget_width_px > 0 ? widget_width_px : 0;
}

constexpr int ClientX(int screen_x, int taskbar_screen_left)
{
    return screen_x - taskbar_screen_left;
}

constexpr int SnapLeft(int start_left, int window_width)
{
    const int position = start_left - window_width - 2;
    return position > 0 ? position : 0;
}

constexpr int Vertical(int taskbar_height, int start_height, int window_height, int offset)
{
    if (start_height <= 0 || start_height > taskbar_height)
        start_height = taskbar_height;
    int position = (start_height - window_height) / 2 + (taskbar_height - start_height);
    const int maximum = taskbar_height > window_height ? taskbar_height - window_height : 0;
    if (position < 0)
        position = 0;
    if (position > maximum)
        position = maximum;
    return position + offset; // Explicit user offsets may intentionally exceed the automatic bounds.
}
} // namespace TaskbarPosition
