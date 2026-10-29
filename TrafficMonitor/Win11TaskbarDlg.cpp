#include "stdafx.h"
#include "Win11TaskbarDlg.h"
#include "WindowsSettingHelper.h"
#include "TaskbarPosition.h"

void CWin11TaskbarDlg::AdjustTaskbarWndPos(bool force_adjust)
{
    if (m_window_width <= 0 || m_window_height <= 0)
        return;
    // Explorer can replace child windows without replacing the taskbar itself.
    m_hNotify = ::FindWindowEx(m_hTaskbar, nullptr, L"TrayNotifyWnd", nullptr);
    m_hStart = ::FindWindowEx(m_hTaskbar, nullptr, L"Start", nullptr);
    m_rcNotify.SetRectEmpty();
    m_rcStart.SetRectEmpty();
    const bool notify_valid = ::GetWindowRect(m_hNotify, m_rcNotify) != FALSE && !m_rcNotify.IsRectEmpty();
    const bool start_valid = ::GetWindowRect(m_hStart, m_rcStart) != FALSE && m_rcStart.Height() > 0;
    if (start_valid)
        m_rcStart.OffsetRect(-m_rcTaskbar.left, -m_rcTaskbar.top);
    const bool center_align = CWindowsSettingHelper::IsTaskbarCenterAlign();
    const bool widgets_shown = CWindowsSettingHelper::IsTaskbarWidgetsBtnShown();
    const int widget_space = TaskbarPosition::LeftEdge(theApp.m_taskbar_data.avoid_overlap_with_widgets,
        widgets_shown, DPI(theApp.m_taskbar_data.taskbar_left_space_win11));

    //设置窗口大小
    m_rect.right = m_rect.left + m_window_width;
    m_rect.bottom = m_rect.top + m_window_height;
    {
        //任务窗口显示在右侧时，或者Windows11下任务栏左对齐时
        //（Windows11下，如果任务栏设置为左对齐，即使在“任务栏窗口设置”中设置了任务窗口显示在左边，窗口仍然显示在右边）
        if (!theApp.m_taskbar_data.tbar_wnd_on_left || !center_align)
        {
            ////靠近任务栏图标的情况
            //if (theApp.m_taskbar_data.tbar_wnd_snap && IsTaskbarCloseToIconEnable(theApp.m_taskbar_data.tbar_wnd_on_left))
            //{
            //    m_rect.MoveToX(m_rcMin.right + 2);
            //}
            ////靠近通知区的情况
            //else
            //{
            //通知区窗口的水平位置
            int notify_x_pos = TaskbarPosition::ClientX(m_rcNotify.left, m_rcTaskbar.left);
            //没有获取到通知区位置的情况
            if (!notify_valid)
            {
                //Win11副屏没有通知区窗口，这里使用固定的值（88像素的系统时间区域）
                if (m_is_secondary_display)
                    notify_x_pos = m_rcTaskbar.Width() - DPI(88);
                //如果不是副屏，但是仍然没有获取到通知区域的位置，使用配置文件中taskbar_right_space_win11指定的值
                else
                    notify_x_pos = m_rcTaskbar.Width() - DPI(theApp.m_taskbar_data.taskbar_right_space_win11);
            }
            //如果显示了小组件，并且任务栏靠左显示，则留出小组件的位置
            if (theApp.m_taskbar_data.avoid_overlap_with_widgets && widgets_shown && !center_align)
                m_rect.MoveToX(notify_x_pos - m_rect.Width() + 2 - widget_space);
            else
                m_rect.MoveToX(notify_x_pos - m_rect.Width() + 2);
            //}
        }
        //任务栏窗口显示在左侧时
        else
        {
            //靠近“开始”按钮
            if (theApp.m_taskbar_data.tbar_wnd_snap && start_valid)
            {
                m_rect.MoveToX(TaskbarPosition::SnapLeft(m_rcStart.left, m_rect.Width()));
            }
            //靠近最左侧
            else
            {
                m_rect.MoveToX(widget_space);
            }
        }
        //水平偏移
        m_rect.MoveToX(m_rect.left + DPI(theApp.m_taskbar_data.window_offset_left));
        ////确保水平方向不超出屏幕边界
        //if (m_rect.left < 0)
        //    m_rect.MoveToX(0);
        //if (m_rcTaskbar.Width() > m_rect.Width() && m_rect.right > m_rcTaskbar.Width())
        //    m_rect.MoveToX(m_rcTaskbar.Width() - m_rect.Width());

        //设置任务栏窗口的垂直位置
        //注：这里加上(m_rcTaskbar.Height() - rcStart.Height())用于修正Windows11 build 22621版本后触屏设备任务栏窗口位置不正确的问题。
        //在这种情况下m_rcTaskbar的高度要大于m_rcBar的高度，正常情况下，它们的高度相同
        //但是当任务栏上没有任何图标时，m_rcBar的高度会变为0，因此使用rcStart代替
        m_rect.MoveToY(TaskbarPosition::Vertical(m_rcTaskbar.Height(), start_valid ? m_rcStart.Height() : 0,
            m_rect.Height(), DPI(theApp.m_taskbar_data.window_offset_top)));
        ////确保垂直方向不超出屏幕边界
        //if (m_rect.top < 0)
        //    m_rect.MoveToY(0);
        //if (m_rcTaskbar.Height() > m_rect.Height() && m_rect.bottom > m_rcTaskbar.Height())
        //    m_rect.MoveToY(m_rcTaskbar.Height() - m_rect.Height());

        // Recalculate every tick, but only move when actual layout inputs change.
        // This also handles widget toggles, offsets and same-width notification moves.
        const TaskbarPosition::Placement target{m_rect.left, m_rect.top, m_rect.Width(), m_rect.Height()};
        if (m_placement_cache.NeedsMove(target, force_adjust))
        {
            const bool moved = ::MoveWindow(GetSafeHwnd(), m_rect.left, m_rect.top,
                m_rect.Width(), m_rect.Height(), TRUE) != FALSE;
            m_placement_cache.Commit(target, moved);
        }
    }
}

void CWin11TaskbarDlg::InitTaskbarWnd()
{
    m_placement_cache.Reset();
    m_hNotify = ::FindWindowEx(m_hTaskbar, 0, L"TrayNotifyWnd", NULL);
    m_hStart = ::FindWindowEx(m_hTaskbar, nullptr, L"Start", NULL);
    ::GetWindowRect(m_hNotify, m_rcNotify);
}

void CWin11TaskbarDlg::ResetTaskbarPos()
{
}

HWND CWin11TaskbarDlg::GetParentHwnd()
{
    return m_hTaskbar;
}

void CWin11TaskbarDlg::CheckTaskbarOnTopOrBottom()
{
    m_taskbar_on_top_or_bottom = true;
}
